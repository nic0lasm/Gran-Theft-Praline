// Side-by-side stills of two gameplay movies at the same times: "before" on the left,
// "after" on the right, one JPEG per step. macOS only (AVFoundation, no ffmpeg needed).
//
//   swift compare-stills.swift <before.mov> <after.mov> <out_dir> [step_s=10] [width=1280]
import AVFoundation
import CoreGraphics
import CoreText
import Foundation
import ImageIO
import UniformTypeIdentifiers

let args = CommandLine.arguments
guard args.count >= 4 else {
    FileHandle.standardError.write("usage: compare-stills.swift before.mov after.mov out_dir [step_s] [width]\n".data(using: .utf8)!)
    exit(2)
}
let step = args.count > 4 ? Double(args[4]) ?? 10 : 10
let half = args.count > 5 ? Int(args[5]) ?? 1280 : 1280
let out = URL(fileURLWithPath: args[3], isDirectory: true)
try FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)

func generator(_ path: String) -> (AVAssetImageGenerator, Double) {
    let asset = AVURLAsset(url: URL(fileURLWithPath: path))
    let gen = AVAssetImageGenerator(asset: asset)
    gen.appliesPreferredTrackTransform = true
    gen.requestedTimeToleranceBefore = .zero
    gen.requestedTimeToleranceAfter = .zero
    return (gen, CMTimeGetSeconds(asset.duration))
}

let (genA, durA) = generator(args[1])
let (genB, durB) = generator(args[2])
let duration = min(durA, durB)
guard duration > 0 else { FileHandle.standardError.write("empty movie\n".data(using: .utf8)!); exit(1) }

func label(_ ctx: CGContext, _ text: String, x: CGFloat, h: CGFloat) {
    let font = CTFontCreateWithName("Helvetica-Bold" as CFString, 34, nil)
    let attrs: [NSAttributedString.Key: Any] = [NSAttributedString.Key(kCTFontAttributeName as String): font,
                                                NSAttributedString.Key(kCTForegroundColorAttributeName as String): CGColor(gray: 1, alpha: 1)]
    let line = CTLineCreateWithAttributedString(NSAttributedString(string: text, attributes: attrs))
    ctx.setFillColor(CGColor(gray: 0, alpha: 0.55))
    ctx.fill(CGRect(x: x + 16, y: h - 64, width: 190, height: 48))
    ctx.textPosition = CGPoint(x: x + 28, y: h - 52)
    CTLineDraw(line, ctx)
}

var t = 0.0
var written = 0
while t < duration {
    let time = CMTime(seconds: t, preferredTimescale: 600)
    guard let a = try? genA.copyCGImage(at: time, actualTime: nil),
          let b = try? genB.copyCGImage(at: time, actualTime: nil) else { t += step; continue }
    let h = Int(Double(half) * Double(a.height) / Double(a.width))
    let ctx = CGContext(data: nil, width: half * 2, height: h, bitsPerComponent: 8, bytesPerRow: 0,
                        space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
    ctx.interpolationQuality = .high
    ctx.draw(a, in: CGRect(x: 0, y: 0, width: half, height: h))
    ctx.draw(b, in: CGRect(x: half, y: 0, width: half, height: h))
    label(ctx, "AVANT", x: 0, h: CGFloat(h))
    label(ctx, "APRÈS", x: CGFloat(half), h: CGFloat(h))
    let url = out.appendingPathComponent(String(format: "compare_t%03d.jpg", Int(t)))
    let dest = CGImageDestinationCreateWithURL(url as CFURL, UTType.jpeg.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(dest, ctx.makeImage()!, [kCGImageDestinationLossyCompressionQuality: 0.88] as CFDictionary)
    CGImageDestinationFinalize(dest)
    written += 1
    t += step
}
print("compare-stills: \(written) images in \(out.path)")
exit(written > 0 ? 0 : 1)
