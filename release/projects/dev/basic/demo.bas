10 REM demo.bas - 示例 BASIC 程序
20 PRINT "Hello from BASIC!"
30 FOR I = 1 TO 5
40 PRINT "I = "; I
50 NEXT I
60 GOSUB 200
70 PRINT "Done."
80 END
200 PRINT "In subroutine."
210 RETURN
