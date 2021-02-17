To resolve the following symbol (as explained in this [article](http://harembadi.blogspot.com/2011/06/addr2line-for-windows.html))

>```
> StreamMapping::OpenRead +0x1be 
> [pc:0x00007FFE40E2BD9E func:0x40e2bbe0 lf=0x810 load=IndexFile(0x00007FFE40E20000:0x00007FFE40E3B000)]
>```

1. Retrieve default image base of the DLL

>```
>E:>dumpbin /Headers IndexFile.dll | findstr "image base"
>            1000 base of code
>       180000000 image base (0000000180000000 to 000000018001AFFF)
>            0.00 image version
>           1B000 size of image
>```

2. Compute the default symbol location

>```
>0x00007FFE40E2BD9E - 0x00007FFE40E20000 + 0x000180000000 - 1 = 0x00018000BD9D‬
>```

3. Run the utility

>```
>E:> winaddr2line.exe -e IndexFile.dll  018000BD9E -a -p -s -f
>Load address: 180000000
>0x8000bd9e: TOCManager::ChangeBufSize at tocmanager.cpp:37
>```


See also [dbghelpexamples.html](http://www.debuginfo.com/examples/dbghelpexamples.html).

>```
>E:\workspaces\external\dbghelpexamples>LineFromAddr.exe IndexFile.dll 018000BD9D
>Loading symbols for: IndexFile.dll ...
>Load address: 180000000
>Looking for symbol at address 18000bd9d ...
>Symbol found:
>Symbol: Function  Address: 18000bd50  Size: 129  Name: TOCManager::ChangeBufSize
>File: g:\...\src\tocmanager.cpp
>Line: 36
>Displacement: 45
>```