import psutil
import json
from os.path import join
from threading import Timer
from lib.pubilc_funs import RepeatingTimer

MONITOR_END = 0
MONITOR_CONTINUE = 1


class PSMonitor:
    def __init__(self, log_dir, keyword='vdo', log_freq=0.5, last_time=1):
        self.log_dir = log_dir
        self.keyword = keyword
        self.log_freq = log_freq
        self.last_time = last_time
        self.end_sign = 0
        self.ps_list = []  # target ps list
        self.log_list = []
        self.end_timer = Timer(last_time, self.end)
        self.rtimer = RepeatingTimer(self.log_freq, self.log)
        # list ps
        self.list_ps()

    def list_ps(self):
        for p in psutil.process_iter(attrs=['name']):
            if self.keyword in p.info['name']:
                self.ps_list.append(p)

    def start(self):
        self.end_sign = MONITOR_CONTINUE
        self.rtimer.start()
        if self.last_time > 0:
            self.end_timer.start()

    def end(self):
        self.end_sign = MONITOR_END
        self.rtimer.cancel()
        output = {'log_freq': self.log_freq,
                  'last_time': self.last_time, 'logs': self.log_list}
        with open(join(self.log_dir, 'psmonitor_output.json'), 'w') as f:
            json.dump(output, f)

    def log(self):
        if self.end_sign == MONITOR_CONTINUE:
            ps_status_list = []
            for ps in self.ps_list:
                ps_status_list.append(ps.as_dict())
            self.log_list.append(ps_status_list)

    def set_cpu_affinity(self, first_cpu=1):
        for ps in self.ps_list:
            ps.cpu_affinity([first_cpu])
            first_cpu += 1

    def get_ps_count(self):
        return len(self.ps_list)
