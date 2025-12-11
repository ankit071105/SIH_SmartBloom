# Sunflower Depth Estimator (Raspberry Pi 4)

This project runs a vision pipeline on a Raspberry Pi 4 to detect sunflowers (specifically on phone screens) and rank them by their distance to the camera using Monocular Depth Estimation.

## 1. Hardware Setup
*   **Raspberry Pi 4** (recommended 4GB or 8GB RAM).
*   **Pi Camera Module** (v2.8 or similar) connected via CSI interface.
*   **Test Subjects**: Several smartphones displaying images of sunflowers.
*   **Positioning**: Place the phones at different distances from the camera (e.g., one at 0.5m, another at 1m, another at 1.5m).

## 2. Dependencies
Open a terminal on your Raspberry Pi and install the required Python libraries:

```bash
sudo apt-get update
sudo apt-get install python3-opencv
pip3 install torch torchvision torchaudio --extra-index-url https://download.pytorch.org/whl/cpu
pip3 install timm numpy opencv-python
```

*Note: Determining the correct PyTorch installation for Pi can sometimes be tricky. If the above pip command fails, look for "PyTorch ARM64 wheels" specific to your Pi OS version.*

## 3. The Model File
The script looks for a file used to detect sunflowers:
*   **Filename**: `sunflower_detector.torch`
*   **Location**: Same folder as `depth_sunflower_pi.py`.

**Testing without a trained model**:
If you do not have `sunflower_detector.torch` yet, the script is capable of running in **Simulation Mode**. It will automatically detect **bright yellow regions** (simulating sunflowers) instead of using the AI detector. This allows you to test the depth sorting logic immediately.

## 4. Running the Project
1.  Navigate to the project directory:
    ```bash
    cd /path/to/DepthAnalizer
    ```
2.  Run the script:
    ```bash
    python3 depth_sunflower_pi.py
    ```

## 5. Testing on Laptop / Desktop
**Yes, you can run this exact script on your laptop.**
Since the code uses standard OpenCV webcam access (`cv2.VideoCapture(0)`), it will automatically work with your laptop's built-in webcam.

1.  **Install dependencies** on your PC/Mac:
    ```bash
    pip install opencv-python torch torchvision timm numpy
    ```
2.  **Run the script**:
    ```bash
    python depth_sunflower_pi.py
    ```
3.  **Behavior**:
    *   The script runs on the CPU, just like on the Pi.
    *   It will likely run much smoother (higher FPS) on a laptop than on the Pi 4.
    *   This is the **best way** to test your sunflower detection and depth logic before deploying to the hardware.

## 6. What to Expect
1.  **Normalization**: The system will load `MiDaS-small` (this may take a moment on the first run).
2.  **Live Feed**: A window will open showing the camera feed.
3.  **Detection**:
    *   If using the model: Bounding boxes around phone screens with sunflowers.
    *   If using fallback: Bounding boxes around yellow areas.
4.  **Ranking**:
    *   **#1**: The phone closest to the camera (Green box).
    *   **#2, #3...**: Phones further away (Yellow boxes).

## Troubleshooting
*   **Lag**: The Pi 4 CPU is limited. Frame rate might be low (1-3 FPS) because depth estimation is computationally expensive.
*   **Camera Error**: Ensure the ribbon cable is seated correctly and legacy camera stack is enabled (`sudo raspi-config` -> Interface Options -> Legacy Camera) if you are using an older OS version, or that libcamera is properly configured.
