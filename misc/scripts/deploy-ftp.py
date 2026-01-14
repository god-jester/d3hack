#!/usr/bin/env python3

import ftplib
import os
from typing import Any, Dict, Optional, Tuple, Union

# https://ftputil.sschwarzer.net
import ftputil

# Wrapper to assert the enviroment variable exists.
def getenv(key: str, default: Optional[str] = None, *args: Tuple[Any], **kwargs: Dict[str, Any]) -> str:
    '''Wrapper for os.getenv.'''
    
    value: Union[None, str] = os.getenv(key)
    # Use the value if it exists.
    if value is not None:
        return value
    # If we're provided a default, fallback to that.
    elif default is not None:
        return default
    # Assert key is missing.
    else:
        raise KeyError(f'Missing enviroment variable {key}! Only run this script via make deploy-ftp.')


# Get enviroment variables from build system.
PROGRAM_ID: str = getenv('PROGRAM_ID')
FTP_IP: str = getenv('FTP_IP')
FTP_PORT: int = int(getenv('FTP_PORT'))
FTP_USERNAME: str = getenv('FTP_USERNAME')
OUT: str = getenv('OUT')
SD_OUT: str = getenv('SD_OUT')
# Password may be empty, so special case this.
FTP_PASSWORD: str = getenv('FTP_PASSWORD', '')


class SessionFactory(ftplib.FTP):
    '''Session factory for FTPHost.'''
    
    def __init__(self, ftp_ip: str, ftp_port: int, ftp_username: str, ftp_password: str, *args: Tuple[Any], **kwargs: Dict[str, Any]):
        super().__init__()
        
        # Connect to FTP server.
        self.connect(ftp_ip, ftp_port, timeout=3)
        # Login with credentials.
        self.login(ftp_username, ftp_password)


def main(*args: Tuple[Any], **kwargs: Dict[str, Any]):
    # Connect/login to console FTP server.
    with ftputil.FTPHost(FTP_IP, FTP_PORT, FTP_USERNAME, FTP_PASSWORD, session_factory=SessionFactory) as ftp_host:
        # Make output directory.
        ftp_host.makedirs(SD_OUT, exist_ok=True)

        def upload_tree(local_root: str, remote_root: str) -> None:
            if not os.path.isdir(local_root):
                return
            for dirpath, dirnames, filenames in os.walk(local_root):
                rel_path = os.path.relpath(dirpath, local_root)
                remote_dir = remote_root if rel_path == "." else ftp_host.path.join(remote_root, rel_path)
                ftp_host.makedirs(remote_dir, exist_ok=True)
                for filename in filenames:
                    local_path = os.path.join(dirpath, filename)
                    remote_path = ftp_host.path.join(remote_dir, filename)
                    ftp_host.upload(local_path, remote_path)

        upload_tree(os.path.join(OUT, "exefs"), ftp_host.path.join(SD_OUT, "exefs"))
        upload_tree(os.path.join(OUT, "romfs"), ftp_host.path.join(SD_OUT, "romfs"))


if __name__ == '__main__':
    main()
