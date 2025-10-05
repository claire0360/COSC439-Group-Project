/*
 * Author:   Claire Kim
 * Company:  University of Canterbury COSC439 Group5
 * Date:     20/09/2025
 *
 */

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text.RegularExpressions;
using System.Windows.Forms;

namespace PhotoApp
{
    public partial class Form1 : Form
    {
        private SerialPort _port;

        // ----- Slideshow state -----
        private readonly List<Image> _images = new List<Image>();
        private int _imgIdx = 0;
        private readonly Timer _slideTimer = new Timer();   // advances photos
        private bool _slideshowOn = false;

        // Hysteresis around 3 cm to avoid flicker
        private const double StartThresh = 2.8; // ON when < 2.8 cm
        private const double StopThresh = 3.5; // OFF when > 3.5 cm

        // Debounce
        private readonly TimeSpan _startDebounce = TimeSpan.FromMilliseconds(250);
        private readonly TimeSpan _stopDebounce = TimeSpan.FromMilliseconds(700);
        private DateTime _belowStart = DateTime.MinValue;
        private DateTime _aboveStart = DateTime.MinValue;

        // Smoothing (moving average last N non-NaN samples)
        private readonly Queue<double> _last = new Queue<double>();
        private const int SmoothCount = 5;

        // Your photo folder
        private const string PhotoFolder = @"C:\Users\clair\Desktop\Photos";

        // Regex to pull Sensor B from lines like: "A: 10.23 cm | B: 2.75 cm"
        private static readonly Regex _bRegex = new Regex(
            @"\bB:\s*([-+]?\d+(?:\.\d+)?)\s*(?:cm)?",
            RegexOptions.IgnoreCase | RegexOptions.Compiled);

        public Form1()
        {
            InitializeComponent();

            this.FormClosing += Form1_FormClosing;

            // PictureBox: single frame for slideshow
            pictureBox1.SizeMode = PictureBoxSizeMode.Zoom;
            pictureBox1.BackColor = Color.Black;
            this.DoubleBuffered = true;

            // Slide every 5 seconds
            _slideTimer.Interval = 5000;
            _slideTimer.Tick += (s, e) => AdvancePhoto();

            // UI defaults
            button2.Enabled = false;
            button1.Text = "Connect";
            button2.Text = "Disconnect";

            // Load photos once
            TryLoadImages(PhotoFolder);

            // Ports
            RefreshComPorts();
            comboBox1.DropDown += (s, e) => RefreshComPorts();
        }

        // ------------------ Connect / Disconnect ------------------
        private void button1_Click(object sender, EventArgs e)
        {
            if (_port != null && _port.IsOpen) return;

            var selected = comboBox1.SelectedItem as string;
            if (string.IsNullOrWhiteSpace(selected))
            {
                MessageBox.Show("Please select a COM port first.");
                return;
            }

            try
            {
                _port = new SerialPort(selected, 9600);
                _port.NewLine = "\n";
                _port.ReadTimeout = 2000;
                _port.DataReceived += Port_DataReceived;
                _port.Open();

                button1.Enabled = false;
                button2.Enabled = true;
                comboBox1.Enabled = false;

                button1.Text = "Connected";
                button1.BackColor = Color.LightGreen;
                button2.Text = "Disconnect";
                button2.BackColor = SystemColors.Control;

                this.Text = "RemindME Photos — Connected (" + selected + ")";
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to open " + selected + "\n\n" + ex.Message,
                                "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                SafeClosePort();
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            SafeClosePort();

            button1.Enabled = true;
            button2.Enabled = false;
            comboBox1.Enabled = true;

            button1.Text = "Connect";
            button1.BackColor = SystemColors.Control;
            button2.Text = "Disconnected";
            button2.BackColor = Color.LightCoral;

            this.Text = "RemindME Photos";

            StopSlideshow();
            pictureBox1.Image = null; // clear only on manual disconnect
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            SafeClosePort();
            foreach (var img in _images) img.Dispose();
        }

        private void RefreshComPorts()
        {
            var ports = SerialPort.GetPortNames().OrderBy(p => p).ToArray();
            comboBox1.BeginUpdate();
            comboBox1.Items.Clear();
            comboBox1.Items.AddRange(ports);
            comboBox1.EndUpdate();

            if (comboBox1.Items.Count > 0 && comboBox1.SelectedIndex < 0)
                comboBox1.SelectedIndex = 0;
        }

        private void SafeClosePort()
        {
            try
            {
                if (_port != null)
                {
                    _port.DataReceived -= Port_DataReceived;
                    if (_port.IsOpen) _port.Close();
                    _port.Dispose();
                    _port = null;
                }
            }
            catch { }
        }

        // ------------------ Serial + Sensor B only ------------------
        private void Port_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                var sp = (SerialPort)sender;
                string chunk = sp.ReadExisting();
                if (string.IsNullOrEmpty(chunk)) return;

                // Split into lines and check each for a B: value
                var lines = chunk.Split(new[] { "\r\n", "\n", "\r" }, StringSplitOptions.RemoveEmptyEntries);
                foreach (var line in lines)
                {
                    double b = ParseDistanceB(line);
                    if (double.IsNaN(b)) continue;

                    // Smooth
                    b = Smoothed(b);
                    if (double.IsNaN(b)) continue;

                    // Debounced state machine with hysteresis
                    if (b < StartThresh)
                    {
                        if (_belowStart == DateTime.MinValue) _belowStart = DateTime.UtcNow;
                        _aboveStart = DateTime.MinValue;

                        if (!_slideshowOn && (DateTime.UtcNow - _belowStart) >= _startDebounce)
                            BeginInvoke((Action)StartSlideshow);
                    }
                    else if (b > StopThresh)
                    {
                        if (_aboveStart == DateTime.MinValue) _aboveStart = DateTime.UtcNow;
                        _belowStart = DateTime.MinValue;

                        if (_slideshowOn && (DateTime.UtcNow - _aboveStart) >= _stopDebounce)
                            BeginInvoke((Action)StopSlideshow);
                    }
                    else
                    {
                        // between thresholds — reset edge timers
                        _belowStart = DateTime.MinValue;
                        _aboveStart = DateTime.MinValue;
                    }
                }
            }
            catch
            {
                // ignore transient serial glitches
            }
        }

        // Extracts Sensor B from a line like: "A: 15.2 cm | B: 2.75 cm"
        private static double ParseDistanceB(string line)
        {
            var m = _bRegex.Match(line);
            if (!m.Success) return double.NaN;

            double cm;
            if (double.TryParse(m.Groups[1].Value, NumberStyles.Float,
                                CultureInfo.InvariantCulture, out cm))
                return cm;

            return double.NaN;
        }

        private double Smoothed(double cm)
        {
            if (double.IsNaN(cm)) return double.NaN;
            if (_last.Count == SmoothCount) _last.Dequeue();
            _last.Enqueue(cm);

            double sum = 0; int n = 0;
            foreach (var v in _last) { sum += v; n++; }
            return n > 0 ? sum / n : double.NaN;
        }

        // ------------------ Slideshow control ------------------
        private void StartSlideshow()
        {
            if (_slideshowOn) return;
            _slideshowOn = true;

            if (_images.Count == 0) return;

            _imgIdx = 0;
            pictureBox1.Image = _images[_imgIdx]; // show immediately
            _slideTimer.Start();
        }

        private void StopSlideshow()
        {
            if (!_slideshowOn) return;
            _slideshowOn = false;
            _slideTimer.Stop();
            // Do NOT clear image here—keeps last frame, avoids blanks.
        }

        private void AdvancePhoto()
        {
            if (_images.Count == 0) return;
            _imgIdx = (_imgIdx + 1) % _images.Count;
            pictureBox1.Image = _images[_imgIdx];
        }

        private void TryLoadImages(string folder)
        {
            if (!Directory.Exists(folder)) return;

            var paths = Directory.EnumerateFiles(folder)
                                 .Where(p =>
                                     p.EndsWith(".jpg", StringComparison.OrdinalIgnoreCase) ||
                                     p.EndsWith(".jpeg", StringComparison.OrdinalIgnoreCase) ||
                                     p.EndsWith(".png", StringComparison.OrdinalIgnoreCase));

            foreach (var p in paths)
            {
                using (var src = Image.FromFile(p))
                {
                    _images.Add(new Bitmap(src)); // clone to avoid file lock
                }
            }
        }

        // Designer stubs if present
        private void label1_Click(object sender, EventArgs e) { }
        private void pictureBox2_Click(object sender, EventArgs e) { }
    }
}
