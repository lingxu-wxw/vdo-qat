class VB_CONFIG:
    top_cmd = "top -b -H -n 1"
    grid = [2] # [1, 2]
    log_freq = 0.1
    device = '/dev/nvme0n1'
    test_name = 'test'
    deduplication = 'enabled'
    compression = 'enabled'
   

    def __init__(self):
        pass


vb_config = VB_CONFIG()
