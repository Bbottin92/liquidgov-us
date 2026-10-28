# liquidgov-platform/tools/make_og_card.ps1
# Generates the share card (og:image/twitter:image), windows/.NET System.Drawing only --
# no Pillow, ImageMagick, or any other package is installed anywhere in this dev
# environment (WSL has no pip/apt sudo access; Windows has no PIL either). Run from
# Windows PowerShell:
#
#   powershell -ExecutionPolicy Bypass -File liquidgov-platform/tools/make_og_card.ps1
#
# Writes assets/og-image-v4.jpg (1200x630) next to this script's repo. Bump $OutName
# and every reference to it (page_wrap's og_image in http_server.cpp, commons/index.html's
# four og:image/twitter:image tags, and a new serving route beside the old v3/v2/v1 ones)
# if the text changes again -- crawlers cache og:image near-permanently, so reusing a path
# leaves old shares stuck on stale art forever (the same reason v1 -> v2 -> v3 each moved).
# Edit the text/colors in this file and re-run it; don't hand-patch the JPG's bytes.
Add-Type -AssemblyName System.Drawing

[int]$W = 1200
[int]$H = 630
$bmp = New-Object -TypeName System.Drawing.Bitmap -ArgumentList $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

# Warm gradient background instead of flat paper -- a little more inviting than a
# flat gray-beige card, still restrained (civic, not neon).
$topColor = [System.Drawing.Color]::FromArgb(238, 231, 214)
$botColor = [System.Drawing.Color]::FromArgb(222, 208, 180)
$rect = New-Object -TypeName System.Drawing.Rectangle -ArgumentList 0, 0, $W, $H
$gradBrush = New-Object -TypeName System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList $rect, $topColor, $botColor, 90.0
$g.FillRectangle($gradBrush, $rect)
$gradBrush.Dispose()

$ink = [System.Drawing.Color]::FromArgb(58, 42, 24)
$gold = [System.Drawing.Color]::FromArgb(168, 120, 56)
$goldDark = [System.Drawing.Color]::FromArgb(140, 98, 44)
$muted = [System.Drawing.Color]::FromArgb(96, 82, 60)

# Certificate-style double border -- ties the "civic" idea to something that
# reads as official/inviting rather than a plain slab of text.
$outerPen = New-Object -TypeName System.Drawing.Pen -ArgumentList $gold, 3
$g.DrawRectangle($outerPen, 24, 24, ($W-48), ($H-48))
$outerPen.Dispose()
$innerPen = New-Object -TypeName System.Drawing.Pen -ArgumentList $goldDark, 1
$g.DrawRectangle($innerPen, 34, 34, ($W-68), ($H-68))
$innerPen.Dispose()

$inkBrush = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $ink
$mutedBrush = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $muted
$goldBrush = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $gold

$center = New-Object -TypeName System.Drawing.StringFormat
$center.Alignment = [System.Drawing.StringAlignment]::Center

[single]$halfW = $W / 2.0

# Headline
$headlineFont = New-Object -TypeName System.Drawing.Font -ArgumentList "Georgia", 58, ([System.Drawing.FontStyle]::Bold)
$headlinePoint = New-Object -TypeName System.Drawing.PointF -ArgumentList $halfW, 190
$g.DrawString("This is YOUR government!", $headlineFont, $inkBrush, $headlinePoint, $center)

# Subhead -- gold, letter-spaced caps, reads like a seal/stamp under the headline
$subFont = New-Object -TypeName System.Drawing.Font -ArgumentList "Georgia", 32, ([System.Drawing.FontStyle]::Bold)
$sub = "REPRESENT YOURSELF"
$spaced = ($sub.ToCharArray() -join [char]0x2002)
$subPoint = New-Object -TypeName System.Drawing.PointF -ArgumentList $halfW, 300
$g.DrawString($spaced, $subFont, $goldBrush, $subPoint, $center)

# Short accent rule under the subhead, not a full-width line -- a small flourish,
# not a hard divider, so the card doesn't read as a dry form.
$accentPen = New-Object -TypeName System.Drawing.Pen -ArgumentList $gold, 2
$g.DrawLine($accentPen, ($halfW - 60), 365, ($halfW + 60), 365)
$accentPen.Dispose()

# A one-line invitation instead of nothing between the hook and the footer --
# what actually makes this a civic project worth clicking into, not just a slogan.
$inviteFont = New-Object -TypeName System.Drawing.Font -ArgumentList "Georgia", 22, ([System.Drawing.FontStyle]::Italic)
$invitePoint = New-Object -TypeName System.Drawing.PointF -ArgumentList $halfW, 410
$g.DrawString("Organize. Collaborate. Build what comes next.", $inviteFont, $mutedBrush, $invitePoint, $center)

# Footer rule
$rulePen = New-Object -TypeName System.Drawing.Pen -ArgumentList $gold, 2
$g.DrawLine($rulePen, 88, 500, ($W-88), 500)
$rulePen.Dispose()

# Footer text
$wordmarkFont = New-Object -TypeName System.Drawing.Font -ArgumentList "Georgia", 22, ([System.Drawing.FontStyle]::Regular)
$wordmarkPoint = New-Object -TypeName System.Drawing.PointF -ArgumentList 88, 540
$g.DrawString("LiquidGov.US", $wordmarkFont, $inkBrush, $wordmarkPoint)

$taglineFont = New-Object -TypeName System.Drawing.Font -ArgumentList "Georgia", 20, ([System.Drawing.FontStyle]::Italic)
$taglineText = "A Voluntary Natural Democracy"
$taglineSize = $g.MeasureString($taglineText, $taglineFont)
[single]$taglineX = $W - 88 - $taglineSize.Width
$taglinePoint = New-Object -TypeName System.Drawing.PointF -ArgumentList $taglineX, 542
$g.DrawString($taglineText, $taglineFont, $mutedBrush, $taglinePoint)

$g.Flush()

$jpegCodec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
$encParams = New-Object -TypeName System.Drawing.Imaging.EncoderParameters -ArgumentList 1
$qualityParam = New-Object -TypeName System.Drawing.Imaging.EncoderParameter -ArgumentList ([System.Drawing.Imaging.Encoder]::Quality), ([int64]92)
$encParams.Param[0] = $qualityParam
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outPath = Join-Path $repoRoot "liquidgov-platform\assets\og-image-v4.jpg"
$bmp.Save($outPath, $jpegCodec, $encParams)

$g.Dispose(); $bmp.Dispose()
"saved: $outPath"
