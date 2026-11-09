// Turns the numbered JPEG frames written by the game's -CaptureVideo switch
// into an H.264 QuickTime movie, with the system frameworks only.
//
// Usage: swift tools/build/frames-to-movie.swift <frames_dir> <fps> <output.mov>

import AVFoundation
import CoreGraphics
import Foundation
import ImageIO

func fail(_ message: String) -> Never {
    FileHandle.standardError.write("frames-to-movie: \(message)\n".data(using: .utf8)!)
    exit(1)
}

let args = CommandLine.arguments
guard args.count == 4, let fps = Int32(args[2]), fps > 0 else {
    fail("usage: frames-to-movie.swift <frames_dir> <fps> <output.mov>")
}
let framesDir = URL(fileURLWithPath: args[1])
let outputURL = URL(fileURLWithPath: args[3])

let frameURLs = ((try? FileManager.default.contentsOfDirectory(at: framesDir, includingPropertiesForKeys: nil)) ?? [])
    .filter { $0.lastPathComponent.hasPrefix("frame_") && $0.pathExtension == "jpg" }
    .sorted { $0.lastPathComponent < $1.lastPathComponent }
guard !frameURLs.isEmpty else { fail("no frame_*.jpg in \(framesDir.path)") }

func loadImage(_ url: URL) -> CGImage? {
    guard let source = CGImageSourceCreateWithURL(url as CFURL, nil) else { return nil }
    return CGImageSourceCreateImageAtIndex(source, 0, nil)
}

guard let first = loadImage(frameURLs[0]) else { fail("cannot read \(frameURLs[0].path)") }
let width = first.width
let height = first.height

try? FileManager.default.removeItem(at: outputURL)
guard let writer = try? AVAssetWriter(outputURL: outputURL, fileType: .mov) else { fail("cannot create \(outputURL.path)") }

let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264,
    AVVideoWidthKey: width,
    AVVideoHeightKey: height,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: 8_000_000],
])
input.expectsMediaDataInRealTime = false
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA,
    kCVPixelBufferWidthKey as String: width,
    kCVPixelBufferHeightKey as String: height,
])
writer.add(input)
guard writer.startWriting() else { fail("writer failed: \(String(describing: writer.error))") }
writer.startSession(atSourceTime: .zero)

func pixelBuffer(from image: CGImage) -> CVPixelBuffer? {
    guard let pool = adaptor.pixelBufferPool else { return nil }
    var buffer: CVPixelBuffer?
    CVPixelBufferPoolCreatePixelBuffer(nil, pool, &buffer)
    guard let pixels = buffer else { return nil }
    CVPixelBufferLockBaseAddress(pixels, [])
    defer { CVPixelBufferUnlockBaseAddress(pixels, []) }
    guard let context = CGContext(
        data: CVPixelBufferGetBaseAddress(pixels), width: width, height: height, bitsPerComponent: 8,
        bytesPerRow: CVPixelBufferGetBytesPerRow(pixels), space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue
    ) else { return nil }
    context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
    return pixels
}

var written = 0
for (index, url) in frameURLs.enumerated() {
    guard let image = loadImage(url), image.width == width, image.height == height,
          let buffer = pixelBuffer(from: image) else {
        FileHandle.standardError.write("skipping \(url.lastPathComponent)\n".data(using: .utf8)!)
        continue
    }
    while !input.isReadyForMoreMediaData { usleep(1_000) }
    if adaptor.append(buffer, withPresentationTime: CMTime(value: CMTimeValue(index), timescale: fps)) {
        written += 1
    }
}

input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()
guard writer.status == .completed else { fail("writer failed: \(String(describing: writer.error))") }
print("\(outputURL.path): \(written) frames, \(width)x\(height), \(fps) fps")
