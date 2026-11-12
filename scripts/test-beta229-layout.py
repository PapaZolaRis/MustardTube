#!/usr/bin/env python3
"""Compatibility entrypoint: current uniform-card layout contract is Beta 2.3.0."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('test-beta230-layout.py')), run_name='__main__')
