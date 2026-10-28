# nx-ovlloader
Host process for loading Switch overlay OVLs (NROs)

This is the loader service of the Tesla ecosystem. It's derrived from the default nx-hbloader.
When being run, this service automatically tries to chainload `/switch/.overlays/ovlmenu.ovl`, the Tesla Menu. From there on it will
load and switch between different overlays on request. 
