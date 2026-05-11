# Downloads the FFMPEG-Kit files compiled using `--speed` by Github CI
# Script by Fern ;)


def check_imports():
    import_error = False
    try:
        import github as _
    except ImportError:
        print("Missing PyGithub! (pip install PyGithub)")
        import_error = True
    try:
        import requests as _
    except ImportError:
        print("Missing requests! (pip install requests)")
        import_error = True
    return not import_error


if not check_imports():
    exit(1)


from github import Github
import json
import os
import requests
import shutil
import zipfile

repository = Github().get_repo("Fernthedev/ffmpeg-kit-speed")
release = repository.get_release("v4.4.1")  # Update as needed

libraries_asset_name = "ffmpeg-kit.aar"
headers_asset_name = "ffmpeg-kit-headers.zip"

ffmpeg_folder = os.path.join(os.getcwd(), "ffmpeg")
temp_folder = os.path.join(os.getcwd(), "ffmpeg_temp")

assets = release.get_assets()

with open("./mod.template.json", "r") as f:
    modjson = json.load(f)


def download_zip(asset_name: str):
    print(f"Downloading {asset_name}")
    for asset in assets:
        if asset.name == asset_name:
            zip_file = asset.name
            r = requests.get(asset.browser_download_url, allow_redirects=True)
            with open(zip_file, "wb") as f:
                f.write(r.content)
            break
    else:
        print("Unable to find asset:", asset_name)
        exit(1)


if os.path.exists(temp_folder):
    print("Clearing temp folder!")
    shutil.rmtree(temp_folder)

print("Downloading zips")
download_zip(libraries_asset_name)
download_zip(headers_asset_name)

print("Clearing old FFMPEG folder")
# Clear folder
if os.path.isfile(ffmpeg_folder):
    os.remove(ffmpeg_folder)
shutil.rmtree(ffmpeg_folder)

print("Creating folders")
# Create folder
os.makedirs(temp_folder)
os.makedirs(ffmpeg_folder)


def unzip_file(zip_name: str):
    print(f"Unzipping {zip_name}")
    # Unzip
    with zipfile.ZipFile(zip_name, "r") as z:
        # Extract all the contents of zip file in current directory
        z.extractall(temp_folder)


print("Unzipping files")
unzip_file(headers_asset_name)
unzip_file(libraries_asset_name)

# Reorganize files
print("Moving library files")
temp_libs = os.path.join(os.path.join(temp_folder, "jni"), "arm64-v8a")
for filename in os.listdir(temp_libs):
    if filename not in modjson["libraryFiles"]:
        continue
    file_path = os.path.join(temp_libs, filename)
    print(f"Moving file {file_path} -> {ffmpeg_folder}")
    os.rename(file_path, os.path.join(ffmpeg_folder, filename))

print("Moving header files")
temp_headers = os.path.join(temp_folder, "include")
for filename in os.listdir(temp_headers):
    file_path = os.path.join(temp_headers, filename)
    print(f"Moving file {file_path} -> {ffmpeg_folder}")
    os.rename(file_path, os.path.join(ffmpeg_folder, filename))

# Delete temp files
print("Cleaning up")
os.remove(libraries_asset_name)
os.remove(headers_asset_name)
shutil.rmtree(temp_folder)
