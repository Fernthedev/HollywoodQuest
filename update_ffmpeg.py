# Downloads the FFMPEG-Kit files compiled using `--speed` by Github CI
# Script by Fern ;)

# pip install PyGithub
from github import Github

# pip install requests
import requests  # For downloading files

import os
import shutil
import zipfile

repository = Github().get_repo('Fernthedev/ffmpeg-kit-speed')

release = repository.get_release("v4.4.1")  # Update as needed

releaseLibrariesAssetName = "ffmpeg-kit.aar"
releaseHeadersAssetName = "ffmpeg-kit-headers.zip"

ffmpegFolder = os.path.join(os.getcwd(), "ffmpeg")
ffmpegTempFolder = os.path.join(os.getcwd(), "ffmpeg_temp")

assets = release.get_assets()


def download_zip(asset_name):
    print(f"Downloading {asset_name}")
    zip_file = ""

    # Find FFMPEG and download it
    for asset in assets:
        if asset.name == asset_name:
            zip_file = asset.name
            r = requests.get(asset.browser_download_url, allow_redirects=True)
            open(zip_file, 'wb').write(r.content)

    if zip_file == "":
        raise Exception('Unable to find zip file ' + asset_name)


if os.path.exists(ffmpegTempFolder):
    print("Clearing temp folder!")
    shutil.rmtree(ffmpegTempFolder)

print("Downloading zips")
download_zip(releaseLibrariesAssetName)
download_zip(releaseHeadersAssetName)

print("Clearing folder")
# Clear folder
if os.path.isfile(ffmpegFolder):
    os.remove(ffmpegFolder)

print("Creating folders")
# Create folder
os.makedirs(ffmpegFolder, exist_ok=True)
os.makedirs(ffmpegTempFolder, exist_ok=True)


def unzip_file(zip_name):
    print(f"Unzipping {zip_name}")
    # Unzip
    with zipfile.ZipFile(zip_name, 'r') as zipObj:
        # Extract all the contents of zip file in current directory
        zipObj.extractall(ffmpegTempFolder)


print("Unzipping files")
unzip_file(releaseHeadersAssetName)
unzip_file(releaseLibrariesAssetName)

print("Clearing old FFMPEG folder")
shutil.rmtree(ffmpegFolder)
os.makedirs(ffmpegFolder)

# Reorganize files
print("Moving library files")
ffmpegTempLibs = os.path.join(os.path.join(ffmpegTempFolder, "jni"), "arm64-v8a")
for filename in os.listdir(ffmpegTempLibs):
    file_path = os.path.join(ffmpegTempLibs, filename)
    print(f"Moving file {file_path} to {ffmpegFolder}")
    os.rename(file_path, os.path.join(ffmpegFolder, filename))

print("Moving header files")
ffmpegTempHeaders = os.path.join(ffmpegTempFolder, "include")
for filename in os.listdir(ffmpegTempHeaders):
    file_path = os.path.join(ffmpegTempHeaders, filename)
    print(f"Moving file {file_path} to {ffmpegFolder}")
    os.rename(file_path, os.path.join(ffmpegFolder, filename))

# Delete temp files
print("Cleaning up zip files!")
os.remove(releaseLibrariesAssetName)
os.remove(releaseHeadersAssetName)

print("No more temp folder!")
shutil.rmtree(ffmpegTempFolder)
