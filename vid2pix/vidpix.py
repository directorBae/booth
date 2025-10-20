# -*- coding: utf-8 -*-
import cv2
import numpy as np
import struct
import hashlib
import os
import sys

def create_binary_video_for_arduino(input_path, output_bin_path, target_width=180, target_height=100, color_depth=24):
    """
    Create binary video file optimized for Arduino/ESP32 with NeoPixel LED matrices.
    
    Binary File Structure (Arduino Compatible):
    - Header (16 bytes):
      * Width (4 bytes, uint32 little-endian)
      * Height (4 bytes, uint32 little-endian) 
      * FPS (4 bytes, float32 little-endian)
      * Frame count (4 bytes, uint32 little-endian)
    - Frame data:
      * Each frame: width * height * 3 bytes (BGR format for NeoPixels)
      * Total per frame: 180 * 100 * 3 = 54,000 bytes (24-bit)
    - Trailer (32 bytes):
      * SHA-256 checksum of all frame data (for integrity verification)
    
    Total file size = 16 + (frameCount * 54000) + 32 bytes
    
    Args:
        input_path: Input video file path
        output_bin_path: Output binary file path for Arduino
        target_width: Target width (default: 180)
        target_height: Target height (default: 100)
        color_depth: Color depth (24 for full RGB, 16 for 565 format)
    """
    
    cap = cv2.VideoCapture(input_path)
    
    if not cap.isOpened():
        print(f"Error: Cannot open video file: {input_path}")
        return False
    
    # Get video information
    original_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    original_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    
    print(f"\n=== Creating Arduino-Compatible Binary File ===")
    print(f"Output: {output_bin_path}")
    print(f"Original: {original_width}x{original_height} -> Target: {target_width}x{target_height}")
    print(f"FPS: {fps}, Total frames: {total_frames}")
    print(f"Color depth: {color_depth}-bit")
    
    # Calculate parameters for processing
    scale_x = original_width / target_width
    scale_y = original_height / target_height
    kernel_size = max(3, int(max(scale_x, scale_y) * 1.5))
    if kernel_size % 2 == 0:
        kernel_size += 1
    kernel_size = min(kernel_size, 15)
    
    # Prepare binary file data
    frame_data = bytearray()
    frame_count = 0
    
    print("\nProcessing frames...")
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        # Calculate original brightness and contrast
        original_mean = np.mean(frame)
        original_std = np.std(frame)
        
        # Apply Gaussian blur and resize
        blurred_frame = cv2.GaussianBlur(frame, (kernel_size, kernel_size), 0)
        resized_frame = cv2.resize(blurred_frame, (target_width, target_height), 
                                   interpolation=cv2.INTER_AREA)
        
        # Enhanced brightness and contrast compensation
        resized_mean = np.mean(resized_frame)
        resized_std = np.std(resized_frame)
        
        if resized_mean > 0 and resized_std > 0:
            # Normalize to match original statistics
            normalized_frame = (resized_frame - resized_mean) / resized_std
            brightness_compensated = normalized_frame * original_std + original_mean
            brightness_compensated = np.clip(brightness_compensated, 0, 255)
        else:
            # Fallback: simple brightness scaling
            brightness_factor = original_mean / max(resized_mean, 1)
            brightness_compensated = resized_frame * min(brightness_factor, 3.0)
            brightness_compensated = np.clip(brightness_compensated, 0, 255)
        
        # Apply adaptive sharpening (reduced intensity)
        kernel_sharpen = np.array([[-0.2, -0.2, -0.2],
                                   [-0.2,  2.6, -0.2], 
                                   [-0.2, -0.2, -0.2]])
        final_frame = cv2.filter2D(brightness_compensated, -1, kernel_sharpen)
        
        # Add gamma correction for additional brightness boost
        gamma = 1.2
        gamma_corrected = np.power(final_frame / 255.0, 1.0 / gamma) * 255.0
        final_frame = np.clip(gamma_corrected, 0, 255).astype(np.uint8)
        
        # Convert to target color depth if needed
        if color_depth == 16:
            # Convert 24-bit BGR to 16-bit RGB565
            frame_16bit = cv2.cvtColor(final_frame, cv2.COLOR_BGR2RGB)
            frame_565 = ((frame_16bit[:,:,0] >> 3) << 11) | ((frame_16bit[:,:,1] >> 2) << 5) | (frame_16bit[:,:,2] >> 3)
            frame_bytes = frame_565.astype(np.uint16).tobytes()
        else:
            # Keep 24-bit BGR format
            frame_bytes = final_frame.tobytes()
        
        # Add frame data to binary array
        frame_data.extend(frame_bytes)
        frame_count += 1
        
        if frame_count % 30 == 0 or frame_count == total_frames:
            progress = (frame_count / total_frames) * 100
            print(f"\rProgress: {frame_count}/{total_frames} ({progress:.1f}%)", end='', flush=True)
    
    cap.release()
    
    print("\n")
    
    # Calculate checksum
    checksum = hashlib.sha256(frame_data).digest()
    
    # Write binary file
    with open(output_bin_path, 'wb') as f:
        # Header (16 bytes) - little-endian format compatible with Arduino
        f.write(struct.pack('<I', target_width))      # 4 bytes: width (uint32_t)
        f.write(struct.pack('<I', target_height))     # 4 bytes: height (uint32_t)
        f.write(struct.pack('<f', fps))               # 4 bytes: fps (float)
        f.write(struct.pack('<I', frame_count))       # 4 bytes: frame count (uint32_t)
        
        # Frame data
        f.write(frame_data)
        
        # Trailer (32 bytes): SHA-256 checksum for integrity verification
        f.write(checksum)
    
    # Calculate file statistics
    bytes_per_frame = target_width * target_height * (color_depth // 8)
    file_size = len(frame_data) + 48
    
    print(f"✓ Binary file created successfully!")
    print(f"  File: {output_bin_path}")
    print(f"  Frames: {frame_count}")
    print(f"  Bytes per frame: {bytes_per_frame:,}")
    print(f"  Total file size: {file_size:,} bytes ({file_size / (1024*1024):.2f} MB)")
    print(f"  Checksum: {checksum.hex()}")
    print(f"\n✓ Ready for Arduino/ESP32-C5!")
    
    return True

def resize_video_with_gaussian(input_path, output_path, target_width=180, target_height=100):
    """
    Resize video with Gaussian filtering to minimize quality loss.
    
    Args:
        input_path: Input video file path
        output_path: Output video file path
        target_width: Target width (default: 180)
        target_height: Target height (default: 100)
    """
    
    # Create video capture object
    cap = cv2.VideoCapture(input_path)
    
    if not cap.isOpened():
        print(f"Error: Cannot open video file: {input_path}")
        return
    
    # Get original video information
    original_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    original_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    
    print(f"Original resolution: {original_width}x{original_height}")
    print(f"Target resolution: {target_width}x{target_height}")
    print(f"FPS: {fps}")
    print(f"Total frames: {total_frames}")
    
    # Calculate downsampling ratio
    scale_x = original_width / target_width
    scale_y = original_height / target_height
    print(f"Downsampling ratio: x={scale_x:.2f}, y={scale_y:.2f}")
    
    # Calculate Gaussian kernel size based on scale ratio
    # Set appropriate kernel size according to Nyquist-Shannon sampling theorem
    kernel_size = max(3, int(max(scale_x, scale_y) * 1.5))
    if kernel_size % 2 == 0:  # Make it odd
        kernel_size += 1
    kernel_size = min(kernel_size, 15)  # Maximum size limit
    
    print(f"Gaussian kernel size: {kernel_size}x{kernel_size}")
    
    # Setup video writer with better codec compatibility
    # Try different codecs in order of preference
    codecs_to_try = ['X264', "XVID", 'MJPG', 'mp4v']
    out = None
    
    for codec in codecs_to_try:
        fourcc = cv2.VideoWriter_fourcc(*codec)
        out = cv2.VideoWriter(output_path, fourcc, fps, (target_width, target_height))
        if out.isOpened():
            print(f"Using codec: {codec}")
            break
        else:
            out.release()
    
    if out is None or not out.isOpened():
        print(f"Error: Cannot create output video file with any codec: {output_path}")
        cap.release()
        return
    
    frame_count = 0
    
    print("\nProcessing...")
    
    while True:
        ret, frame = cap.read()
        
        if not ret:
            break
        
        # Calculate original brightness and contrast
        original_mean = np.mean(frame)
        original_std = np.std(frame)
        
        # Apply Gaussian blur
        blurred_frame = cv2.GaussianBlur(frame, (kernel_size, kernel_size), 0)
        
        # Resize frame
        resized_frame = cv2.resize(blurred_frame, (target_width, target_height), 
                                   interpolation=cv2.INTER_AREA)
        
        # Enhanced brightness and contrast compensation
        resized_mean = np.mean(resized_frame)
        resized_std = np.std(resized_frame)
        
        if resized_mean > 0 and resized_std > 0:
            # Normalize to match original statistics
            normalized_frame = (resized_frame - resized_mean) / resized_std
            brightness_compensated = normalized_frame * original_std + original_mean
            brightness_compensated = np.clip(brightness_compensated, 0, 255)
        else:
            # Fallback: simple brightness scaling
            brightness_factor = original_mean / max(resized_mean, 1)
            brightness_compensated = resized_frame * min(brightness_factor, 3.0)  # Cap at 3x
            brightness_compensated = np.clip(brightness_compensated, 0, 255)
        
        # Apply adaptive sharpening (reduced intensity)
        kernel_sharpen = np.array([[-0.2, -0.2, -0.2],
                                   [-0.2,  2.6, -0.2], 
                                   [-0.2, -0.2, -0.2]])
        final_frame = cv2.filter2D(brightness_compensated, -1, kernel_sharpen)
        
        # Add gamma correction for additional brightness boost
        gamma = 1.2  # Values > 1.0 make image brighter
        gamma_corrected = np.power(final_frame / 255.0, 1.0 / gamma) * 255.0
        final_frame = np.clip(gamma_corrected, 0, 255).astype(np.uint8)
        
        out.write(final_frame)
        
        frame_count += 1
        # Progress display
        if frame_count % 30 == 0 or frame_count == total_frames:
            progress = (frame_count / total_frames) * 100
            print(f"\rProgress: {frame_count}/{total_frames} ({progress:.1f}%)", end='')
    
    print(f"\n\nCompleted! Processed {frame_count} frames.")
    print(f"Output file: {output_path}")
    
    # Release resources
    cap.release()
    out.release()
    cv2.destroyAllWindows()
    
    # Verify output file was created and has size > 0
    import os
    if os.path.exists(output_path) and os.path.getsize(output_path) > 0:
        print(f"Success! Output file size: {os.path.getsize(output_path)} bytes")
    else:
        print("Warning: Output file may not have been created properly")


if __name__ == "__main__":
    # Parse command line arguments
    if len(sys.argv) < 3:
        print("Usage: python vidpix.py <input_file> <output_basename>")
        print("\nExample:")
        print("  python vidpix.py F1.mp4 output_video")
        print("  python vidpix.py image.jpg output_image")
        print("\nOutput files:")
        print("  - output_video.mp4 (full resolution)")
        print("  - output_video_1.mp4 to output_video_10.mp4 (180x8 tiles)")
        print("  - output_video_1.bin to output_video_10.bin (binary files)")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_basename = sys.argv[2]
    
    # Validate input file exists
    if not os.path.exists(input_file):
        print(f"Error: Input file not found: {input_file}")
        sys.exit(1)
    
    print("=== Video/Image Processing with Tiling ===")
    print(f"Input: {input_file}")
    print(f"Output basename: {output_basename}")
    
    # Load input (video or image)
    print("\n[1/4] Loading input file...")
    
    cap = cv2.VideoCapture(input_file)
    
    if cap.isOpened():
        # It's a video file
        is_video = True
        original_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        original_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        fps = cap.get(cv2.CAP_PROP_FPS)
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        
        print(f"  Video detected: {original_width}x{original_height}, {fps}fps, {total_frames} frames")
        
        # Read all frames
        frames = []
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            frames.append(frame)
        cap.release()
    else:
        # Try to load as image
        frames = [cv2.imread(input_file)]
        if frames[0] is None:
            print(f"Error: Cannot load input file as video or image: {input_file}")
            sys.exit(1)
        
        is_video = False
        original_height, original_width = frames[0].shape[:2]
        fps = 30.0
        total_frames = 1
        
        print(f"  Image detected: {original_width}x{original_height}")
    
    # Determine target resolution
    print("\n[2/4] Determining target resolution...")
    
    # Check if input is already at target resolution or needs scaling
    target_width = 180
    target_height = 100
    
    # Handle different input sizes
    if original_width != target_width or original_height != target_height:
        # Calculate aspect ratio
        aspect_ratio = original_width / original_height
        target_aspect = target_width / target_height
        
        if abs(aspect_ratio - target_aspect) < 0.01:
            # Aspect ratio matches, just scale
            print(f"  Input aspect ratio matches target, scaling to {target_width}x{target_height}")
        else:
            # Scale to maintain aspect ratio
            scale = min(target_width / original_width, target_height / original_height)
            target_width = int(original_width * scale)
            target_height = int(original_height * scale)
            print(f"  Input aspect ratio differs, scaling to {target_width}x{target_height}")
    
    print(f"  Target resolution: {target_width}x{target_height}")
    
    # Process all frames
    print(f"\n[3/4] Processing frames...")
    
    processed_frames = []
    
    for frame_idx, frame in enumerate(frames):
        # Calculate original brightness and contrast
        original_mean = np.mean(frame)
        original_std = np.std(frame)
        
        # Resize to target resolution
        if frame.shape[1] != target_width or frame.shape[0] != target_height:
            scale_x = frame.shape[1] / target_width
            scale_y = frame.shape[0] / target_height
            kernel_size = max(3, int(max(scale_x, scale_y) * 1.5))
            if kernel_size % 2 == 0:
                kernel_size += 1
            kernel_size = min(kernel_size, 15)
            
            # Apply Gaussian blur before resizing
            blurred_frame = cv2.GaussianBlur(frame, (kernel_size, kernel_size), 0)
            resized_frame = cv2.resize(blurred_frame, (target_width, target_height), 
                                      interpolation=cv2.INTER_AREA)
        else:
            resized_frame = frame.copy()
        
        # Enhanced brightness and contrast compensation
        resized_mean = np.mean(resized_frame)
        resized_std = np.std(resized_frame)
        
        if resized_mean > 0 and resized_std > 0:
            normalized_frame = (resized_frame - resized_mean) / resized_std
            brightness_compensated = normalized_frame * original_std + original_mean
            brightness_compensated = np.clip(brightness_compensated, 0, 255)
        else:
            brightness_factor = original_mean / max(resized_mean, 1)
            brightness_compensated = resized_frame * min(brightness_factor, 3.0)
            brightness_compensated = np.clip(brightness_compensated, 0, 255)
        
        # Apply adaptive sharpening
        kernel_sharpen = np.array([[-0.2, -0.2, -0.2],
                                   [-0.2,  2.6, -0.2], 
                                   [-0.2, -0.2, -0.2]])
        final_frame = cv2.filter2D(brightness_compensated, -1, kernel_sharpen)
        
        # Add gamma correction
        gamma = 1.2
        gamma_corrected = np.power(final_frame / 255.0, 1.0 / gamma) * 255.0
        final_frame = np.clip(gamma_corrected, 0, 255).astype(np.uint8)
        
        processed_frames.append(final_frame)
        
        if (frame_idx + 1) % max(1, len(frames) // 10) == 0 or frame_idx == len(frames) - 1:
            print(f"  Progress: {frame_idx + 1}/{len(frames)}")
    
    # Save full resolution video
    print(f"\n[4/4] Generating output files...")
    
    full_output_video = f"{output_basename}.mp4"
    print(f"\n  Creating full video: {full_output_video}")
    
    codecs_to_try = ['X264', "XVID", 'MJPG', 'mp4v']
    out = None
    
    for codec in codecs_to_try:
        fourcc = cv2.VideoWriter_fourcc(*codec)
        out = cv2.VideoWriter(full_output_video, fourcc, fps, (target_width, target_height))
        if out.isOpened():
            print(f"    Using codec: {codec}")
            break
        else:
            out.release()
    
    if out and out.isOpened():
        for frame in processed_frames:
            out.write(frame)
        out.release()
        print(f"    ✓ {full_output_video} created")
    else:
        print(f"    ✗ Failed to create video file")
    
    # Create 10 tiles (8 lines each = 180x8 resolution)
    tile_height = 8
    num_tiles = 10
    
    print(f"\n  Creating {num_tiles} tiled videos ({target_width}x{tile_height} each):")
    
    for tile_idx in range(num_tiles):
        start_y = tile_idx * tile_height
        end_y = min(start_y + tile_height, target_height)
        
        tile_width = target_width
        tile_actual_height = end_y - start_y
        
        # Create tiled video
        tile_video_name = f"{output_basename}_{tile_idx + 1}.mp4"
        
        out = None
        for codec in codecs_to_try:
            fourcc = cv2.VideoWriter_fourcc(*codec)
            out = cv2.VideoWriter(tile_video_name, fourcc, fps, (tile_width, tile_actual_height))
            if out.isOpened():
                break
            else:
                out.release()
        
        if out and out.isOpened():
            for frame in processed_frames:
                tile_frame = frame[start_y:end_y, :]
                out.write(tile_frame)
            out.release()
            print(f"    ✓ {tile_video_name} ({tile_width}x{tile_actual_height})")
        
        # Create tiled binary file
        tile_bin_name = f"{output_basename}_{tile_idx + 1}.bin"
        
        frame_data = bytearray()
        for frame in processed_frames:
            tile_frame = frame[start_y:end_y, :]
            frame_data.extend(tile_frame.tobytes())
        
        checksum = hashlib.sha256(frame_data).digest()
        
        with open(tile_bin_name, 'wb') as f:
            f.write(struct.pack('<I', tile_width))           # width
            f.write(struct.pack('<I', tile_actual_height))   # height
            f.write(struct.pack('<f', fps))                  # fps
            f.write(struct.pack('<I', len(processed_frames)))# frame count
            f.write(frame_data)
            f.write(checksum)
        
        file_size = len(frame_data) + 48
        print(f"    ✓ {tile_bin_name} ({file_size:,} bytes)")
    
    print(f"\n=== Processing Complete ===")
    print(f"Output files created:")
    print(f"  - {full_output_video} (full video)")
    for i in range(1, num_tiles + 1):
        print(f"  - {output_basename}_{i}.mp4 (video tile)")
        print(f"  - {output_basename}_{i}.bin (binary tile)")
