#!/usr/bin/env python3
"""
ESP32 Temperature Logger - USB Serial Log Manager
Communicates with ESP32 via USB serial port to manage log files.
"""

import serial
import serial.tools.list_ports
import sys
import os
import time
from typing import Optional, List, Tuple

class ESP32LogManager:
    def __init__(self, port: Optional[str] = None, baudrate: int = 115200):
        """
        Initialize ESP32 Log Manager.
        
        Args:
            port: Serial port name (e.g., 'COM3' or '/dev/ttyUSB0'). 
                  If None, will attempt to auto-detect.
            baudrate: Serial baud rate (default 115200)
        """
        self.port = port
        self.baudrate = baudrate
        self.serial: Optional[serial.Serial] = None
        
    def find_esp32_port(self) -> Optional[str]:
        """Auto-detect ESP32 serial port."""
        ports = serial.tools.list_ports.comports()
        
        for port in ports:
            # Look for common ESP32 USB identifiers
            if 'CP210' in port.description or 'CH340' in port.description or \
               'USB' in port.description or 'UART' in port.description:
                print(f"Found potential ESP32 at: {port.device} ({port.description})")
                return port.device
        
        return None
    
    def connect(self) -> bool:
        """Connect to ESP32."""
        if self.port is None:
            self.port = self.find_esp32_port()
            if self.port is None:
                print("Error: Could not auto-detect ESP32 port.")
                print("\nAvailable ports:")
                for port in serial.tools.list_ports.comports():
                    print(f"  {port.device}: {port.description}")
                return False
        
        try:
            self.serial = serial.Serial(self.port, self.baudrate, timeout=2)
            time.sleep(0.5)  # Give ESP32 time to reset/initialize
            
            # Clear any buffered data
            self.serial.reset_input_buffer()
            self.serial.reset_output_buffer()
            
            print(f"Connected to {self.port} at {self.baudrate} baud")
            return True
        except serial.SerialException as e:
            print(f"Error connecting to {self.port}: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from ESP32."""
        if self.serial and self.serial.is_open:
            self.serial.close()
            print("Disconnected")
    
    def send_command(self, command: str) -> List[str]:
        """
        Send command to ESP32 and receive response.
        
        Args:
            command: Command string to send
            
        Returns:
            List of response lines
        """
        if not self.serial or not self.serial.is_open:
            print("Error: Not connected")
            return []
        
        # Send command
        self.serial.write((command + '\n').encode())
        self.serial.flush()
        
        # Read response until OK or ERROR
        response_lines = []
        start_time = time.time()
        timeout = 10  # 10 second timeout
        
        while True:
            if time.time() - start_time > timeout:
                print("Error: Command timeout")
                break
            
            if self.serial.in_waiting > 0:
                line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    response_lines.append(line)
                    
                    # Check for command completion
                    if line.startswith('OK') or line.startswith('ERROR'):
                        break
        
        return response_lines
    
    def list_logs(self) -> List[Tuple[str, int]]:
        """
        List all log files on ESP32.
        
        Returns:
            List of tuples (filename, size_bytes)
        """
        print("Listing log files...")
        response = self.send_command('LIST')
        
        files = []
        in_list = False
        
        for line in response:
            if line == '--- LOG FILES ---':
                in_list = True
            elif line == '--- END LIST ---':
                in_list = False
            elif in_list and '.csv' in line:
                # Parse: "log_0001.csv (1234 bytes)"
                parts = line.split(' (')
                if len(parts) == 2:
                    filename = parts[0].strip()
                    size_str = parts[1].replace(' bytes)', '').strip()
                    try:
                        size = int(size_str)
                        files.append((filename, size))
                    except ValueError:
                        pass
        
        return files
    
    def get_log(self, filename: str, output_path: Optional[str] = None) -> bool:
        """
        Download a log file from ESP32.
        
        Args:
            filename: Name of log file to download
            output_path: Local path to save file. If None, uses same filename.
            
        Returns:
            True if successful
        """
        if output_path is None:
            output_path = filename.lstrip('/')
        
        print(f"Downloading {filename}...")
        response = self.send_command(f'GET {filename}')
        
        # Extract file content between markers
        content_lines = []
        in_file = False
        
        for line in response:
            if line.startswith('--- BEGIN FILE:'):
                in_file = True
            elif line.startswith('--- END FILE:'):
                in_file = False
            elif in_file:
                content_lines.append(line)
        
        if content_lines:
            try:
                with open(output_path, 'w', encoding='utf-8') as f:
                    f.write('\n'.join(content_lines) + '\n')
                print(f"Saved to {output_path}")
                return True
            except IOError as e:
                print(f"Error saving file: {e}")
                return False
        else:
            print(f"Error: No content received for {filename}")
            return False
    
    def get_status(self) -> dict:
        """
        Get system status from ESP32.
        
        Returns:
            Dictionary with status information
        """
        print("Getting system status...")
        response = self.send_command('STATUS')
        
        status = {}
        
        for line in response:
            if ':' in line and not line.startswith('---'):
                parts = line.split(':', 1)
                key = parts[0].strip()
                value = parts[1].strip()
                status[key] = value
        
        return status
    
    def delete_log(self, filename: str) -> bool:
        """
        Delete a log file from ESP32.
        
        Args:
            filename: Name of log file to delete, or '*' for all
            
        Returns:
            True if successful
        """
        if filename == '*':
            confirm = input("Delete ALL log files? (yes/no): ")
            if confirm.lower() != 'yes':
                print("Cancelled")
                return False
        
        print(f"Deleting {filename}...")
        response = self.send_command(f'DEL {filename}')
        
        for line in response:
            print(line)
        
        # Check if successful
        return any('OK' in line for line in response)
    
    def download_all_logs(self, output_dir: str = '.') -> int:
        """
        Download all log files from ESP32.
        
        Args:
            output_dir: Directory to save files
            
        Returns:
            Number of files downloaded
        """
        files = self.list_logs()
        
        if not files:
            print("No log files found")
            return 0
        
        print(f"\nFound {len(files)} log files")
        
        # Create output directory if needed
        if output_dir != '.' and not os.path.exists(output_dir):
            os.makedirs(output_dir)
        
        downloaded = 0
        for filename, size in files:
            output_path = os.path.join(output_dir, filename.lstrip('/'))
            if self.get_log(filename, output_path):
                downloaded += 1
        
        print(f"\nDownloaded {downloaded}/{len(files)} files")
        return downloaded


def main():
    """Main CLI interface."""
    import argparse
    
    parser = argparse.ArgumentParser(description='ESP32 Temperature Logger Manager')
    parser.add_argument('-p', '--port', help='Serial port (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('-b', '--baudrate', type=int, default=115200, help='Baud rate (default: 115200)')
    
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    
    # List command
    subparsers.add_parser('list', help='List all log files')
    
    # Get command
    get_parser = subparsers.add_parser('get', help='Download a log file')
    get_parser.add_argument('filename', help='Log file name')
    get_parser.add_argument('-o', '--output', help='Output file path')
    
    # Get-all command
    getall_parser = subparsers.add_parser('get-all', help='Download all log files')
    getall_parser.add_argument('-d', '--dir', default='.', help='Output directory')
    
    # Status command
    subparsers.add_parser('status', help='Get system status')
    
    # Delete command
    del_parser = subparsers.add_parser('delete', help='Delete a log file')
    del_parser.add_argument('filename', help='Log file name or * for all')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    # Create manager and connect
    manager = ESP32LogManager(port=args.port, baudrate=args.baudrate)
    
    if not manager.connect():
        return 1
    
    try:
        # Execute command
        if args.command == 'list':
            files = manager.list_logs()
            if files:
                print(f"\n{'Filename':<20} {'Size':>10}")
                print('-' * 32)
                for filename, size in files:
                    print(f"{filename:<20} {size:>10} bytes")
            else:
                print("No log files found")
        
        elif args.command == 'get':
            manager.get_log(args.filename, args.output)
        
        elif args.command == 'get-all':
            manager.download_all_logs(args.dir)
        
        elif args.command == 'status':
            status = manager.get_status()
            print("\nSystem Status:")
            print('-' * 40)
            for key, value in status.items():
                print(f"{key:<25} {value}")
        
        elif args.command == 'delete':
            manager.delete_log(args.filename)
    
    finally:
        manager.disconnect()
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
