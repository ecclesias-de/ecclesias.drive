
app_name=@APPLICATION_SHORTNAME@
github_repo_dir="/Users/chingencheng/$app_name/"

cd 

git clone --depth=1 https://invent.kde.org/kde/craftmaster.git CraftMaster/CraftMaster

python3 CraftMaster/CraftMaster/CraftMaster.py --config "./$app_name/.craft.ini" --variables "WORKSPACE=$github_repo_dir"

python3 CraftMaster/macos-64-clang/craft/bin/craft.py --unshelve "$github_repo_dir.craft.shelf"

python3 CraftMaster/macos-64-clang/craft/bin/craft.py --install-deps $app_name
# make sure remove owap before build
python3 CraftMaster/macos-64-clang/craft/bin/craft.py --no-cache --src-dir "$github_repo_dir" $app_name
# need to create complete app first 
sudo python3 CraftMaster/macos-64-clang/craft/bin/craft.py --no-cache --src-dir "$github_repo_dir" --package "$app_name"
