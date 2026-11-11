using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;

class IconCreator
{
    static void Main()
    {
        int size = 256;
        Bitmap bmp = new Bitmap(size, size);
        using (Graphics g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            
            // Background - dark gradient
            using (var brush = new LinearGradientBrush(
                new Point(0, 0), new Point(size, size),
                Color.FromArgb(40, 40, 60), Color.FromArgb(20, 10, 30)))
            {
                g.FillRectangle(brush, 0, 0, size, size);
            }
            
            // Red circle border
            using (var pen = new Pen(Color.FromArgb(180, 20, 20), 8))
            {
                g.DrawEllipse(pen, 16, 16, size - 32, size - 32);
            }
            
            // Inner circle
            using (var brush = new SolidBrush(Color.FromArgb(60, 10, 10)))
            {
                g.FillEllipse(brush, 24, 24, size - 48, size - 48);
            }
            
            // DMC text
            using (var font = new Font("Arial Black", 72, FontStyle.Bold, GraphicsUnit.Pixel))
            using (var brush = new SolidBrush(Color.FromArgb(220, 220, 230)))
            {
                StringFormat sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
                g.DrawString("DMC", font, brush, new RectangleF(0, 0, size, size), sf);
            }
            
            // "1" text
            using (var font = new Font("Arial Black", 48, FontStyle.Bold, GraphicsUnit.Pixel))
            using (var brush = new SolidBrush(Color.FromArgb(200, 50, 50)))
            {
                StringFormat sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
                g.DrawString("1", font, brush, new RectangleF(0, size * 0.55f, size, size * 0.45f), sf);
            }
        }
        
        bmp.Save("icon.ico", System.Drawing.Imaging.ImageFormat.Icon);
        Console.WriteLine("icon.ico created successfully!");
        Console.WriteLine($"Size: {new FileInfo("icon.ico").Length} bytes");
    }
}
