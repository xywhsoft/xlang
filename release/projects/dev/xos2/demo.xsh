# xos2 boot script - demonstrates the OS shell
mkdir /home
cd /home
echo hello > test.txt
cat test.txt
ls -l
ps
mem
echo world >> test.txt
cat test.txt
sleep 2
echo boot script done
