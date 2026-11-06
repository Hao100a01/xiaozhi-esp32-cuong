param([string]$LogoSource = "")
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$assetRoot = Join-Path $PSScriptRoot '../main/boards/UTA-robot/assets'
[System.IO.Directory]::CreateDirectory($assetRoot) | Out-Null
$original = Join-Path $assetRoot 'atfuta-original.png'
if ($LogoSource) { Copy-Item -LiteralPath $LogoSource -Destination $original }
if (!(Test-Path -LiteralPath $original)) { throw 'Thiếu ảnh ATFUTA gốc.' }
# Chỉ raster hóa ảnh/font thành dữ liệu giao diện; không gọi công cụ build ESP-IDF.
Add-Type -ReferencedAssemblies System.Drawing.Common,System.Drawing.Primitives,System.Linq,System.Collections,System.Runtime,System.Text.Encoding.Extensions,System.ComponentModel.Primitives,System.Private.Windows.Core,System.Private.Windows.GdiPlus -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.Linq;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
public static class UtaUiAssets {
    static Color C(int rgb) { return Color.FromArgb(255,(rgb>>16)&255,(rgb>>8)&255,rgb&255); }
    static Graphics G(Bitmap b) {
        var g=Graphics.FromImage(b); g.SmoothingMode=SmoothingMode.AntiAlias;
        g.InterpolationMode=InterpolationMode.HighQualityBicubic;
        g.TextRenderingHint=TextRenderingHint.AntiAliasGridFit; return g;
    }
    static void Text(Graphics g,string s,int size,int color,float x,float y,string family="Arial") {
        using(var f=new Font(family,size,FontStyle.Regular,GraphicsUnit.Pixel))
        using(var brush=new SolidBrush(C(color))) g.DrawString(s,f,brush,x,y,StringFormat.GenericTypographic);
    }
    static void Raw565(Bitmap b,string path) {
        using(var writer=new BinaryWriter(File.Create(path)))
        for(int y=0;y<b.Height;y++) for(int x=0;x<b.Width;x++) {
            var c=b.GetPixel(x,y); writer.Write((ushort)(((c.R>>3)<<11)|((c.G>>2)<<5)|(c.B>>3)));
        }
    }
    static void Cloud(Graphics g) {
        using(var b=new SolidBrush(C(0xbad7e6))) {
            g.FillEllipse(b,5,12,17,13); g.FillEllipse(b,13,9,13,15); g.FillEllipse(b,19,15,10,10);
        }
    }
    static Bitmap Icon(int kind) {
        var b=new Bitmap(32,32); using(var g=G(b)) {
            g.Clear(Color.Transparent);
            using(var pen=new Pen(C(0x6ee8ef),2)) using(var yellow=new SolidBrush(C(0xf8d777))) {
                pen.StartCap=LineCap.Round; pen.EndCap=LineCap.Round;
                if(kind==1||kind==3) {
                    g.FillEllipse(yellow,kind==1?9:3,kind==1?9:3,14,14);
                    if(kind==1) for(int i=0;i<8;i++) {double a=i*Math.PI/4;
                        g.DrawLine(pen,(float)(16+11*Math.Cos(a)),(float)(16+11*Math.Sin(a)),
                            (float)(16+14*Math.Cos(a)),(float)(16+14*Math.Sin(a)));}
                }
                if(kind==2) { using(var p=new GraphicsPath()) {
                    p.AddArc(5,4,23,23,65,280); p.AddArc(13,0,19,22,280,-240); p.CloseFigure(); g.FillPath(yellow,p); }
                }
                if(kind>=3&&kind<=8) Cloud(g);
                if(kind==5) {g.DrawLine(pen,4,26,28,26);g.DrawLine(pen,8,30,24,30);}
                if(kind==6) {g.DrawLine(pen,10,26,8,30);g.DrawLine(pen,19,26,17,30);g.DrawLine(pen,27,26,25,30);}
                if(kind==7) {g.DrawLine(pen,12,26,12,31);g.DrawLine(pen,9,28,15,28);g.DrawLine(pen,23,26,23,31);g.DrawLine(pen,20,28,26,28);}
                if(kind==8) {g.FillPolygon(yellow,new Point[]{new Point(18,20),new Point(12,27),new Point(18,27),new Point(15,32),new Point(24,24),new Point(18,24)});}
                if(kind==9||kind==10) {pen.Color=C(kind==9?0x64dfc5:0x8b9baa);
                    g.DrawArc(pen,3,8,26,22,218,104);g.DrawArc(pen,9,14,14,13,218,104);
                    using(var dot=new SolidBrush(pen.Color))g.FillEllipse(dot,14,24,4,4);
                    if(kind==10)g.DrawLine(pen,5,4,28,29);
                }
                if(kind==11) {g.DrawEllipse(pen,8,3,16,18);g.DrawLine(pen,9,17,16,29);g.DrawLine(pen,23,17,16,29);g.DrawEllipse(pen,13,8,6,6);}
                if(kind==0) {g.DrawEllipse(pen,5,5,22,22);Text(g,"?",19,0xbad7e6,11,5);}
            }
        } return b;
    }
    static void FontAsset(string path,string name,int size,bool numeric) {
        var chars=new SortedSet<int>();
        if(numeric)foreach(char c in "0123456789:-°")chars.Add(c);
        else { for(int c=32;c<=126;c++)chars.Add(c);for(int c=160;c<=383;c++)chars.Add(c);
            foreach(int c in new[]{416,417,431,432})chars.Add(c);
            for(int c=7840;c<=7929;c++)chars.Add(c); }
        int height=size+8, baseline=4;
        var bits=new List<byte>(); var descriptors=new List<string>{"{}"};
        using(var font=new Font(numeric&&size>40?"Segoe UI Light":"Arial",size,FontStyle.Regular,GraphicsUnit.Pixel))
        foreach(int cp in chars)using(var bitmap=new Bitmap(size*2+12,height))using(var g=G(bitmap)) {
            g.Clear(Color.Black);string s=char.ConvertFromUtf32(cp);
            g.DrawString(s,font,Brushes.White,0,0,StringFormat.GenericTypographic);
            int left=bitmap.Width,top=height,right=-1,bottom=-1;
            for(int y=0;y<height;y++)for(int x=0;x<bitmap.Width;x++)if(bitmap.GetPixel(x,y).R>8){left=Math.Min(left,x);top=Math.Min(top,y);right=Math.Max(right,x);bottom=Math.Max(bottom,y);}
            int advance;
            using(var format=(StringFormat)StringFormat.GenericTypographic.Clone()) {
                format.FormatFlags|=StringFormatFlags.MeasureTrailingSpaces;
                advance=(int)Math.Round(g.MeasureString(s,font,1000,format).Width*16);
            }
            int w=right<0?0:right-left+1,h=bottom<0?0:bottom-top+1,offset=bits.Count;
            int packed=0,n=0;
            for(int y=0;y<h;y++)for(int x=0;x<w;x++) {int alpha=bitmap.GetPixel(left+x,top+y).R>>4;
                if((n++&1)==0)packed=alpha<<4;else bits.Add((byte)(packed|alpha));}
            if((n&1)!=0)bits.Add((byte)packed);
            descriptors.Add("{.bitmap_index="+offset+", .adv_w="+advance+", .box_w="+w+", .box_h="+h+", .ofs_x="+(w==0?0:left)+", .ofs_y="+(h==0?0:height-baseline-top-h)+"}");
        }
        var code=new StringBuilder("// Generated by scripts/prepare_uta_ui_assets.ps1; rendered glyphs only.\n");
        code.Append("static const uint8_t "+name+"_bitmap[]={\n");
        for(int i=0;i<bits.Count;i++){code.Append("0x"+bits[i].ToString("x2")+",");if(i%24==23)code.AppendLine();}code.Append("\n};\n");
        code.Append("static const lv_font_fmt_txt_glyph_dsc_t "+name+"_glyphs[]={\n"+string.Join(",\n",descriptors)+"\n};\n");
        code.Append("static const uint16_t "+name+"_unicode[]={"+string.Join(",",chars.Select(c=>(c-32).ToString()))+"};\n");
        code.Append("static const lv_font_fmt_txt_cmap_t "+name+"_cmap[]={ {.range_start=32,.range_length="+(chars.Max-31)+",.glyph_id_start=1,.unicode_list="+name+"_unicode,.glyph_id_ofs_list=nullptr,.list_length="+chars.Count+",.type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY} };\n");
        code.Append("static const lv_font_fmt_txt_dsc_t "+name+"_dsc={.glyph_bitmap="+name+"_bitmap,.glyph_dsc="+name+"_glyphs,.cmaps="+name+"_cmap,.kern_dsc=nullptr,.kern_scale=0,.cmap_num=1,.bpp=4,.kern_classes=0,.bitmap_format=0};\n");
        code.Append("const lv_font_t "+name+"={.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height="+height+",.base_line="+baseline+",.subpx=LV_FONT_SUBPX_NONE,.underline_position=-1,.underline_thickness=1,.dsc=&"+name+"_dsc};\n");
        File.WriteAllText(path,code.ToString(),new UTF8Encoding(false));
    }
    public static void Generate(string root) {
        using(var logo=Image.FromFile(Path.Combine(root,"atfuta-original.png")))
        using(var boot=new Bitmap(240,240))using(var g=G(boot)) {
            g.Clear(Color.White);g.DrawImage(logo,7,49,226,113);
            boot.Save(Path.Combine(root,"uta_boot.png"),ImageFormat.Png);
            Raw565(boot,Path.Combine(root,"uta_boot.rgb565"));
            Text(g,"ĐANG KHỞI ĐỘNG",12,0x30383c,66,184);g.FillRectangle(Brushes.Black,88,216,64,2);
            boot.Save(Path.Combine(root,"preview-boot.png"),ImageFormat.Png);
        }
        using(var neon=new Bitmap(240,240)) {
            for(int y=0;y<240;y++)for(int x=0;x<240;x++) {double t=Math.Max(0,1-Math.Sqrt(Math.Pow((240-x)/280.0,2)+Math.Pow(y/260.0,2)));t=t*t;
                neon.SetPixel(x,y,C(((7+(int)(11*t))<<16)|((16+(int)(37*t))<<8)|(27+(int)(43*t))));}
            neon.Save(Path.Combine(root,"uta_neon.png"),ImageFormat.Png);Raw565(neon,Path.Combine(root,"uta_neon.rgb565"));
            using(var g=G(neon))using(var line=new Pen(C(0x233848),1)){
                Text(g,"ATFUTA",12,0x98b3c8,16,16);using(var wifi=Icon(9))g.DrawImage(wifi,171,14,18,18);
                Text(g,"Wi-Fi",12,0x64dfc5,192,16);Text(g,"08:24",54,0x6ee8ef,13,59,"Segoe UI Light");
                Text(g,"Thứ Năm · 05/11/2026",12,0x9db0c2,16,129);g.DrawLine(line,16,158,224,158);
                using(var pin=Icon(11))g.DrawImage(pin,16,176,15,15);
                Text(g,"TP. Hồ Chí Minh",12,0xeef8ff,36,176);
                using(var cloud=Icon(3))g.DrawImage(cloud,15,199,26,26);
                Text(g,"Ít mây",12,0x9db0c2,47,205);Text(g,"28°",30,0xeef8ff,177,193);
            }neon.Save(Path.Combine(root,"preview-neon.png"),ImageFormat.Png);
        }
        using(var sheet=new Bitmap(32*12,32))using(var sg=G(sheet))
        using(var writer=new BinaryWriter(File.Create(Path.Combine(root,"uta_icons.argb8888")))){
            for(int i=0;i<12;i++)using(var icon=Icon(i)){
                icon.Save(Path.Combine(root,"icon-"+i+".png"),ImageFormat.Png);sg.DrawImageUnscaled(icon,32*i,0);
                for(int y=0;y<32;y++)for(int x=0;x<32;x++){var c=icon.GetPixel(x,y);writer.Write(c.B);writer.Write(c.G);writer.Write(c.R);writer.Write(c.A);}
            }sheet.Save(Path.Combine(root,"preview-icons.png"),ImageFormat.Png);
        }
        FontAsset(Path.Combine(root,"uta_font_small.inc"),"uta_font_small",12,false);
        FontAsset(Path.Combine(root,"uta_font_clock.inc"),"uta_font_clock",54,true);
        FontAsset(Path.Combine(root,"uta_font_temperature.inc"),"uta_font_temperature",30,true);
    }
}
'@
[UtaUiAssets]::Generate([System.IO.Path]::GetFullPath($assetRoot))
Write-Output 'Đã chuẩn bị logo, nền Neon, 12 biểu tượng, font và bản xem trước 240x240.'
