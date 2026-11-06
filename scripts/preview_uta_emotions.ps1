# Chỉ đọc GIF trong repo và dựng bảng xem trước; không gọi ESP-IDF.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../main/boards/UTA-robot/assets/emotions'))
$manifest = Get-Content -LiteralPath (Join-Path $root 'manifest.json') -Raw | ConvertFrom-Json
$items = @($manifest.emotions)
$sheet = [System.Drawing.Bitmap]::new(720,[int]([Math]::Ceiling($items.Count/4.0)*160))
$canvas = [System.Drawing.Graphics]::FromImage($sheet)
$canvas.Clear([System.Drawing.Color]::FromArgb(24,24,28))
$font = [System.Drawing.Font]::new('Arial',11)
try {
    for ($i=0; $i -lt $items.Count; $i++) {
        $gif = [System.Drawing.Image]::FromFile((Join-Path $root $items[$i].file))
        $frameCanvas = [System.Drawing.Bitmap]::new(240,240)
        $frameGraphics = [System.Drawing.Graphics]::FromImage($frameCanvas)
        try {
            $frames = $gif.GetFrameCount([System.Drawing.Imaging.FrameDimension]::Time)
            if ($frames -ne $items[$i].frames) { throw 'Số khung GIF không khớp manifest.' }
            for ($frame=0; $frame -lt $frames; $frame++) {
                [void]$gif.SelectActiveFrame([System.Drawing.Imaging.FrameDimension]::Time,$frame)
                $frameGraphics.Clear([System.Drawing.Color]::Black)
                $frameGraphics.DrawImage($gif,0,0,240,240)
            }
            [void]$gif.SelectActiveFrame([System.Drawing.Imaging.FrameDimension]::Time,[int]([Math]::Floor($frames/3)))
            $x=($i%4)*180
            $y=[int]([Math]::Floor($i/4))*160
            $canvas.DrawImage($gif,$x+30,$y,120,120)
            $canvas.DrawString($items[$i].id,$font,[System.Drawing.Brushes]::White,$x+9,$y+127)
        } finally {
            $frameGraphics.Dispose()
            $frameCanvas.Dispose()
            $gif.Dispose()
        }
    }
    $sheet.Save((Join-Path $root 'preview-emotions.png'),[System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $canvas.Dispose()
    $font.Dispose()
    $sheet.Dispose()
}
Write-Output 'Đã đọc mọi khung hình của 21 GIF bằng bộ giải mã Windows và tạo bảng xem trước.'
