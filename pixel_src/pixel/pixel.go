//--pixel command line interface for GIFT
package main

import (
	"flag"
	"fmt"
	"image"
	"image/color"
	"image/jpeg"
	"image/png"
	"io"
	"os"
	"github.com/disintegration/gift"
)

//flags vars
var (
	blurvalue, brvalue, contvalue, hvalue, satvalue, gammavalue, sepiavalue, threshvalue float64
	gray, neg, xpose, xverse, fliph, flipv, emboss, edge, sobel                          bool
	res, resfit, resfill, cropspec, cropsize, sigspec, unsharp, colorize, colorbal       string
	rotvalue, minvalue, maxvalue, meanvalue, medvalue, pixelatevalue                     int
)

func main() {
	//prepare flags
	flag.Float64Var(&blurvalue, "blur", 0, "blur value")
	flag.Float64Var(&brvalue, "brightness", -200, "brightness value (-100, 100)")
	flag.Float64Var(&hvalue, "hue", -200, "hue value (-180, 180)")
	flag.Float64Var(&contvalue, "contrast", -200, "contrast value (-100, 100)")
	flag.Float64Var(&satvalue, "saturation", -200, "saturation value (-100, 500)")
	flag.Float64Var(&gammavalue, "gamma", 1, "gamma value (0-100)")
	flag.Float64Var(&sepiavalue, "sepia", -1, "sepia percentage (0-100)")
	flag.Float64Var(&threshvalue, "threshold", 0, "color threshold percentage (0-100)")
	flag.IntVar(&rotvalue, "rotate", 0, "rotate specified degrees counter-clockwise")
	flag.IntVar(&maxvalue, "max", 0, "local maximum (kernel size)")
	flag.IntVar(&minvalue, "min", 0, "local minimum (kernel size)")
	flag.IntVar(&medvalue, "median", 0, "local median filter (kernel size)")
	flag.IntVar(&meanvalue, "mean", 0, "local mean filter (kernel size)")
	flag.IntVar(&pixelatevalue, "pixelate", 0, "pixelate")
	flag.BoolVar(&flipv, "flipv", false, "flip vertical")
	flag.BoolVar(&fliph, "fliph", false, "flip horizontal")
	flag.BoolVar(&gray, "gray", false, "grayscale")
	flag.BoolVar(&neg, "invert", false, "invert")
	flag.BoolVar(&xpose, "transpose", false, "flip horizontally and rotate 90° counter-clockwise")
	flag.BoolVar(&xverse, "transverse", false, " flips vertically and rotate 90° counter-clockwise")
	flag.BoolVar(&emboss, "emboss", false, "emboss")
	flag.BoolVar(&edge, "edge", false, "edge filter")
	flag.BoolVar(&sobel, "sobel", false, "sobel filter")
	flag.StringVar(&res, "resize", "", "resize w,h")
	flag.StringVar(&resfit, "resizefill", "", "resizefill w,h")
	flag.StringVar(&resfill, "resizefit", "", "resizefit w,h")
	flag.StringVar(&cropspec, "crop", "", "crop x1,y1,x2,y2")
	flag.StringVar(&cropsize, "cropsize", "", "crop w h")
	flag.StringVar(&sigspec, "sigmoid", "", "sigmoid contrast (midpoint,factor)")
	flag.StringVar(&unsharp, "unsharp", "", "unsharp mask (sigma,amount,threshold)")
	flag.StringVar(&colorize, "colorize", "", "colorize (hue, saturation, percentage)")
	flag.StringVar(&colorbal, "colorbalance", "", "color balance (%red, %green, %blue)")
	//usage func override
	flag.Usage = func() {
		fmt.Println("Host Application not running, unable to connect.\nPixel v1.0\n")
	}
	flag.Parse()

	var f io.Reader = os.Stdin
	var ferr error
	var fname string
	
	//exit the app, if no args are passed  
	if len(flag.Args()) == 0 {
		fmt.Fprintf(os.Stderr,
		"Host Application not running, unable to connect.\nPixel v1.0\n")
		os.Exit(1);
	}
	if len(flag.Args()) > 0 {
		fname = flag.Args()[0]
		f, ferr = os.Open(fname)
		if ferr != nil {
			fmt.Fprintf(os.Stderr, "%v\n", ferr)
			os.Exit(1)
		}
	}

	src, format, ierr := image.Decode(f)
	if ierr != nil {
		fmt.Fprintf(os.Stderr, "%s: %v\n", fname, ierr)
		os.Exit(2)
	}

	g := gift.New() // initial state

	// stack filters when command flags are set (not default values)
	if blurvalue > 0 {
		g.Add(gift.GaussianBlur(float32(blurvalue)))
	}

	// brightness
	if brvalue >= -100 && brvalue <= 100 {
		g.Add(gift.Brightness(float32(brvalue)))
	}

	// hue
	if hvalue >= -180 && hvalue <= 180 {
		g.Add(gift.Hue(float32(hvalue)))
	}

	// contrast
	if contvalue >= -100 && contvalue <= 100 {
		g.Add(gift.Contrast(float32(contvalue)))
	}

	// saturation
	if satvalue >= -100 && satvalue <= 500 {
		g.Add(gift.Saturation(float32(satvalue)))
	}

	// gamma
	if gammavalue > -1 {
		g.Add(gift.Gamma(float32(gammavalue)))
	}

	// sepia
	if sepiavalue >= 0 && sepiavalue <= 100 {
		g.Add(gift.Sepia(float32(sepiavalue)))
	}

	// median
	if medvalue > 0 && medvalue%1 == 0 {
		g.Add(gift.Median(medvalue, true))
	}

	// mean
	if meanvalue > 0 && meanvalue%1 == 0 {
		g.Add(gift.Mean(meanvalue, true))
	}

	// minimum
	if minvalue > 0 && minvalue%1 == 0 {
		g.Add(gift.Minimum(minvalue, true))
	}

	// maximum
	if maxvalue > 0 && maxvalue%1 == 0 {
		g.Add(gift.Maximum(maxvalue, true))
	}

	// pixelate
	if pixelatevalue > 0 {
		g.Add(gift.Pixelate(pixelatevalue))
	}

	// threshold
	if threshvalue >= 1 && threshvalue <= 100 {
		g.Add(gift.Threshold(float32(threshvalue)))
	}

	// rotate
	if rotvalue > 0 && rotvalue <= 360 {
		switch rotvalue {
		case 90:
			g.Add(gift.Rotate90())
		case 180:
			g.Add(gift.Rotate180())
		case 270:
			g.Add(gift.Rotate270())
		default:
			g.Add(gift.Rotate(float32(rotvalue), color.White, gift.LinearInterpolation))
		}
	}

	// grayscale
	if gray {
		g.Add(gift.Grayscale())
	}

	// invert
	if neg {
		g.Add(gift.Invert())
	}

	// transpose
	if xpose {
		g.Add(gift.Transpose())
	}

	// transverse
	if xverse {
		g.Add(gift.Transverse())
	}

	// flip horizontal
	if fliph {
		g.Add(gift.FlipHorizontal())
	}

	// flip vertical
	if flipv {
		g.Add(gift.FlipVertical())
	}

	// emboss
	if emboss {
		g.Add(gift.Convolution(
			[]float32{-1, -1, 0, -1, 1, 1, 0, 1, 1},
			false, false, false, 0.0))
	}

	// edge detections
	if edge {
		g.Add(gift.Convolution(
			[]float32{-1, -1, -1, -1, 8, -1, -1, -1, -1},
			false, false, false, 0.0))
	}

	// Sobel filter
	if sobel {
		g.Add(gift.Sobel())
	}

	// resize
	if len(res) > 0 {
		var w, h int
		nr, err := fmt.Sscanf(res, "%d,%d", &w, &h)
		if nr != 2 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -resize width,height\n")
			os.Exit(3)
		}
		g.Add(gift.Resize(w, h, gift.LanczosResampling))
	}

	// resize fit
	if len(resfit) > 0 {
		var w, h int
		nr, err := fmt.Sscanf(resfit, "%d,%d", &w, &h)
		if nr != 2 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -resizefit width,height\n")
			os.Exit(3)
		}
		g.Add(gift.ResizeToFit(w, h, gift.LanczosResampling))
	}

	// resize fill
	if len(resfill) > 0 {
		var w, h int
		nr, err := fmt.Sscanf(resfill, "%d,%d", &w, &h)
		if nr != 2 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -resizefill width,height\n")
			os.Exit(3)
		}
		g.Add(gift.ResizeToFill(w, h, gift.LanczosResampling, gift.CenterAnchor))
	}

	// crop
	if len(cropspec) > 0 {
		var x1, y1, x2, y2 int
		nr, err := fmt.Sscanf(cropspec, "%d,%d,%d,%d", &x1, &y1, &x2, &y2)
		if nr != 4 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -crop x1,y1,x2,y2\n")
			os.Exit(4)
		}
		g.Add(gift.Crop(image.Rect(x1, y1, x2, y2)))
	}

	// cropsize
	if len(cropsize) > 0 {
		var w, h int
		nr, err := fmt.Sscanf(cropsize, "%d,%d", &w, &h)
		if nr != 2 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -cropsize w,h")
			os.Exit(4)
		}
		g.Add(gift.CropToSize(w, h, gift.CenterAnchor))
	}

	// unsharp
	if len(unsharp) > 0 {
		var sigma, amount, threshold float32
		nr, err := fmt.Sscanf(unsharp, "%g,%g,%g", &sigma, &amount, &threshold)
		if nr != 3 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -unsharp sigma,amount,threshold\n")
			os.Exit(5)
		}
		g.Add(gift.UnsharpMask(sigma, amount, threshold))
	}

	// sigmoid
	if len(sigspec) > 0 {
		var midpoint, factor float32
		nr, err := fmt.Sscanf(sigspec, "%g,%g", &midpoint, &factor)
		if nr != 2 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -sigma midpoint,factor\n")
			os.Exit(6)
		}
		g.Add(gift.Sigmoid(midpoint, factor))
	}

	// colorize
	if len(colorize) > 0 {
		var chue, csaturation, cpercent float32
		nr, err := fmt.Sscanf(colorize, "%g,%g,%g", &chue, &csaturation, &cpercent)
		if nr != 3 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -colorize hue,saturation,percent\n")
			os.Exit(7)
		}
		g.Add(gift.Colorize(chue, csaturation, cpercent))
	}

	// color balance
	if len(colorbal) > 0 {
		var pctred, pctblue, pctgreen float32
		nr, err := fmt.Sscanf(colorbal, "%g,%g,%g", &pctred, &pctgreen, &pctblue)
		if nr != 3 || err != nil {
			fmt.Fprintf(os.Stderr, "use: -colorbalance %red,%green,%blue\n")
			os.Exit(8)
		}
		g.Add(gift.ColorBalance(pctred, pctgreen, pctblue))
	}

	// make the filtered image, writing to stdout
	dst := image.NewRGBA(g.Bounds(src.Bounds()))
	g.Draw(dst, src)
	switch format {
	case "png":
		png.Encode(os.Stdout, dst)
	case "jpeg":
		jpeg.Encode(os.Stdout, dst, &jpeg.Options{Quality: 100})
	}
}
