import cv2
import torch
import torch.nn.functional as F
import numpy as np
import time
import sys
import os

CAMERA_ID = 0
# Resolution for processing (Pi 4 runs smoother at lower resolutions)
FRAME_WIDTH = 640
FRAME_HEIGHT = 480
# Model paths
DETECTOR_PATH = "sunflower_detector.torch"

def main():
    print("Initializing Raspberry Pi Vision Pipeline...")

    # 1. Setup Device (CPU for Pi 4)
    device = torch.device("cpu")
    print(f"Using device: {device}")

    # 2. Load MiDaS-small Depth Model
    print("Loading MiDaS-small from torch.hub...")
    try:
        # Trust repo for torch.hub
        torch.hub._validate_not_a_forked_repo=lambda a,b,c: True
        midas = torch.hub.load("intel-isl/MiDaS", "MiDaS_small")
        midas.to(device)
        midas.eval()
        
        # Load Transforms
        midas_transforms = torch.hub.load("intel-isl/MiDaS", "transforms").small_transform
    except Exception as e:
        print(f"Error loading MiDaS: {e}")
        print("Ensure you have internet access for the first run or cached models.")
        return

    # 3. Load Sunflower Detector
    print(f"Loading Sunflower Detector from {DETECTOR_PATH}...")
    detector = None
    try:
        if os.path.exists(DETECTOR_PATH):
            detector = torch.load(DETECTOR_PATH, map_location=device)
            detector.to(device)
            detector.eval()
            print("Detector loaded successfully.")
        else:
            print(f"WARNING: '{DETECTOR_PATH}' not found. Pipeline will run but detection will be skipped/mocked.")
            # For demonstration purposes, if the user doesn't have the file yet:
            print("Please ensure your 'sunflower_detector.torch' is in the script directory.")
    except Exception as e:
        print(f"Error loading detector: {e}")
        print("Ensure the model class definitions are available if not using a JIT model.")
        return

    # 4. Initialize Camera
    cap = cv2.VideoCapture(CAMERA_ID)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)
    
    if not cap.isOpened():
        print("Error: Could not open video capture.")
        return

    print("Pipeline started. Press 'q' to exit.")

    while True:
        start_time = time.time()
        
        # Capture frame
        ret, frame = cap.read()
        if not ret:
            print("Failed to grab frame")
            break
            
        # Resize frame if camera ignores set() properties
        # (Optional, ensures consistency)
        if frame.shape[1] != FRAME_WIDTH:
            frame = cv2.resize(frame, (FRAME_WIDTH, FRAME_HEIGHT))

        # Convert to RGB for PyTorch models
        img_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

        # ---------------------------
        # Step A: Depth Estimation
        # ---------------------------
        input_batch = midas_transforms(img_rgb).to(device)

        with torch.no_grad():
            prediction = midas(input_batch)
            
            # Prediction is 1xHxW (inverse depth/disparity)
            # Resize to ORIGINAL frame size
            prediction = F.interpolate(
                prediction.unsqueeze(1),
                size=(frame.shape[0], frame.shape[1]),
                mode="bicubic",
                align_corners=False,
            ).squeeze()
            
            # Convert to numpy
            depth_map = prediction.cpu().numpy()

        # ---------------------------
        # Step B: Sunflower Detection
        # ---------------------------
        detections = [] # Format: (x1, y1, x2, y2)
        
        if detector is not None:
            # Assuming detector takes a normalized tensor or similar
            # Modify this preprocessing based on your specific detector requirements
            # Standard generic preprocessing: (1, 3, H, W) float scaled 0-1
            det_input = torch.from_numpy(img_rgb).permute(2, 0, 1).float() / 255.0
            det_input = det_input.unsqueeze(0).to(device)
            
            with torch.no_grad():
                # Run inference
                # Assuming output is a tensor of shape (N, 4) or (N, 6) [x1, y1, x2, y2, ...]
                res = detector(det_input)
                
                # Check output format flexibility
                if isinstance(res, (list, tuple)):
                    res = res[0] # Take first batch item if list
                
                # Extract boxes
                # We assume the first 4 columns are x1, y1, x2, y2
                if res is not None and len(res) > 0:
                    boxes = res.cpu().numpy()
                    for box in boxes:
                        # Simple validity check
                        if len(box) >= 4:
                            x1, y1, x2, y2 = int(box[0]), int(box[1]), int(box[2]), int(box[3])
                            # Clip to frame dimensions
                            x1 = max(0, x1)
                            y1 = max(0, y1)
                            x2 = min(frame.shape[1], x2)
                            y2 = min(frame.shape[0], y2)
                            
                            # Filter small invalid boxes
                            if x2 > x1 and y2 > y1:
                                detections.append((x1, y1, x2, y2))
        else:
            # Fallback mockup if no detector: Detect bright yellow regions (Simulated Sunflower)
            # This allows the script to be tested even without the model file
            hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
            # Yellow range
            lower_yellow = np.array([20, 100, 100])
            upper_yellow = np.array([30, 255, 255])
            mask = cv2.inRange(hsv, lower_yellow, upper_yellow)
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            for c in contours:
                if cv2.contourArea(c) > 500: # Filter small noise
                    x, y, w, h = cv2.boundingRect(c)
                    detections.append((x, y, x+w, y+h))

        # ---------------------------
        # Step C: Depth Analysis & Ranking
        # ---------------------------
        ranked_sunflowers = []

        for box in detections:
            x1, y1, x2, y2 = box
            
            # Crop the depth map corresponding to the bounding box
            depth_crop = depth_map[y1:y2, x1:x2]
            
            if depth_crop.size > 0:
                # Compute median depth of the crop
                # MiDaS Output: Inverse Depth (Disparity)
                # Higher Value = Closer Objects
                # Lower Value = Farther Objects
                median_depth = np.median(depth_crop)
                ranked_sunflowers.append({
                    "box": box,
                    "depth_score": median_depth 
                })

        # Sort detections:
        # Goal: #1 is Nearest.
        # Nearest = Highest MiDaS score.
        # Sort Descending.
        ranked_sunflowers.sort(key=lambda x: x["depth_score"], reverse=True)

        # ---------------------------
        # Step D: Visualization
        # ---------------------------
        # Draw all
        for i, item in enumerate(ranked_sunflowers):
            box = item["box"]
            x1, y1, x2, y2 = box
            rank = i + 1
            
            # Color: Green for #1, Yellow for others
            color = (0, 255, 0) if rank == 1 else (0, 255, 255)
            
            # Draw box
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            
            # Draw Label: "#1", "#2", etc.
            label = f"#{rank} (Score: {item['depth_score']:.1f})"
            cv2.putText(frame, label, (x1, y1 - 10), 
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

        # Show FPS
        fps = 1.0 / (time.time() - start_time)
        cv2.putText(frame, f"FPS: {fps:.1f}", (10, 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

        cv2.imshow("Sunflower Depth Estimator", frame)
        
        key = cv2.waitKey(1)
        if key & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
