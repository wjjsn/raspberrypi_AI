1. 先确定能用ssh正常连接到树莓派，两台设备需要在同一子网下（连接到同一台路由器）
2. 安装wsl，执行bash脚本需要linux环境
3. 执行`rsync_pi2pc.sh`，将sysroot同步到本机，注意修改脚本中的路径和用户
4. 安装好clang：`sudo apt install clang`
5. VS Code安装好ms-cmake-tools，随便打开一个工程会自动弹提示选择预设，选择完成即可编译
6. 编译完成后，打开命令面板可以将文件自动发送到树莓派，同样注意修改路径。树莓派安装lldb即可远程调试
7. 如果编译失败比较 `./4b/toolchain.cmake` 和 `./5/toolchain.cmake` 逻辑是否相同，跟着5的来。