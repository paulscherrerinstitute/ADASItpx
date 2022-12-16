set_requestfile_path("$($(MODULE)_DIR)db")

# asiTpxConfig (
#    portName      The name of the asyn port driver to be created.
#    hostAddress   The host address of serval server.
#    configFile    The config file.
#    maxBuffers    The maximum number of NDArray buffers that the NDArrayPool for this driver is          
#                  allowed to allocate. Set this to -1 to allow an unlimited number of buffers.                      
#    maxMemory     The maximum amount of memory that the NDArrayPool for this driver is                    
#                  allowed to allocate. Set this to -1 to allow an unlimited amount of memory.                       
#    priority      The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
#    stackSize     The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.    
# )
asiTpxConfig("$(PORT=TPX$(N=1))", "$(CAM_HOST)", "$(CONFIG)")
dbLoadRecords("asiTpx.template","P=$(PREFIX),R=$(RECORD=cam1:),PORT=$(PORT=TPX$(N=1)),ADDR=$(ADDR=0),TIMEOUT=1")

set_pass0_restoreFile("asiTpx_settings$(N=1).sav")
set_pass1_restoreFile("asiTpx_settings$(N=1).sav")

afterInit create_monitor_set,"asiTpx_settings.req",30,"P=$(PREFIX),R=$(RECORD=cam1:)","asiTpx_settings$(N=1).sav"
