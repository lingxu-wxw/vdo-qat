from config import vb_config
from lib.pubilc_funs import run
from threading import Timer


class TopLogger:
    def __init__(self, log_dir, log_freq=0.1, top_cmd=''):
        self.log_dir = log_dir
        self.log_freq = log_freq
        self.top_cmd = top_cmd
        if top_cmd == '':
            self.top_cmd = vb_config.top_cmd
        self.timer = Timer(self.log_freq, self.log)

    def start(self):
        pass

    def end(self):
        pass

    def log(self):
        pass

    def parser(self):
        pass
