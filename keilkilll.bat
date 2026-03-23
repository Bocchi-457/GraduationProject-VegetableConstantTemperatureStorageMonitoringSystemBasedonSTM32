@echo off
REM ----------------------------------------------------------------------------
REM keilkilll.bat
REM 功能：删除项目中所有的 .bak 备份文件
REM 用法：直接运行此批处理文件，会递归删除当前目录及其子目录中的所有 .bak 文件
REM 注意：请谨慎使用，删除后无法恢复
REM 作者：蔬菜恒温库监控系统
REM 日期：2026-03-07
REM ----------------------------------------------------------------------------

REM 删除当前目录及其子目录中的所有 .bak 文件
del *.bak /s

REM 执行完成后暂停，以便查看执行结果
pause
del *.ddk /s
del *.edk /s
del *.lst /s
del *.lnp /s
del *.mpf /s
del *.mpj /s
del *.obj /s
del *.omf /s
::del *.opt /s  ::不允许删除JLINK的设置
del *.plg /s
del *.rpt /s
del *.tmp /s
del *.__i /s
del *.crf /s
del *.o /s
del *.d /s
del *.axf /s
del *.tra /s
del *.dep /s           
del JLinkLog.txt /s

del *.iex /s
del *.htm /s
del *.sct /s
del *.map /s
exit

