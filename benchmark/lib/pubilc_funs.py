import subprocess
import threading


def run(cmd, capture_output=False):
    cmd_list = cmd.split(' ')
    if capture_output:
        stdout = subprocess.PIPE
        output = subprocess.run(cmd_list, stdout=stdout)
        return output.stdout
    subprocess.run(cmd_list)


class RepeatingTimer(threading.Timer):
    def run(self):
        while not self.finished.is_set():
            self.function(*self.args, **self.kwargs)
            self.finished.wait(self.interval)
