lib /def:parking2.def /machine:x86 /out:parking2.lib && cl.exe /EHsc parking2.cpp /DPARKING2_EXPORTS /Fe:parking2.exe /link parking2.lib
