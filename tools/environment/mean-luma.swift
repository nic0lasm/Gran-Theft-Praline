// Mean luma (0-255, Rec. 601 weights) of an image, printed as an integer.
// Fallback of calibrate-cloud-shadow-mac.sh when ffmpeg is not installed.
// Usage: swift tools/environment/mean-luma.swift <image>
import CoreGraphics
import Foundation
import ImageIO

let url = URL(fileURLWithPath: CommandLine.arguments[1])
guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
  print("nan")
  exit(0)
}
let width = image.width / 4, height = image.height / 4
var pixels = [UInt8](repeating: 0, count: width * height * 4)
let context = CGContext(data: &pixels, width: width, height: height, bitsPerComponent: 8,
                        bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
context.interpolationQuality = .medium
context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
var sum = 0.0
for i in 0..<(width * height) {
  sum += 0.299 * Double(pixels[i * 4]) + 0.587 * Double(pixels[i * 4 + 1]) + 0.114 * Double(pixels[i * 4 + 2])
}
print(Int((sum / Double(width * height)).rounded()))
