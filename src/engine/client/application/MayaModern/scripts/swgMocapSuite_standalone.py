"""Launch the SWG mocap suite outside Maya (simulator or Kinect)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from swg_mocap.app import main

if __name__ == "__main__":
    main()
