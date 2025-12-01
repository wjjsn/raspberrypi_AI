mkdir -p ./sysroot/usr/include/
mkdir -p ./sysroot/usr/lib/
mkdir -p ./sysroot/usr/share/
mkdir -p ./sysroot/lib/

rsync -avz wjjsn@pi5.local:/usr/include/ ./sysroot/usr/include/
rsync -avz wjjsn@pi5.local:/usr/lib/ ./sysroot/usr/lib/
rsync -avz wjjsn@pi5.local:/lib/ ./sysroot/lib/
rsync -avz wjjsn@pi5.local:/usr/share/ ./sysroot/usr/share/