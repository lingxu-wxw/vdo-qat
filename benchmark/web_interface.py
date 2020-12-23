from flask import Flask, send_from_directory
from lib.fio_parser import get_fio_speed
from os.path import join
import os


app = Flask(__name__)


@app.route('/')
def index():
    return send_from_directory('html', 'index.html')


@app.route('/detail/<null>')
def detail(null):
    return send_from_directory('html', 'psmonitor.html')


@app.route('/psmonitor/<project>:<result>')
def psmonitor(project, result):
    return send_from_directory(join('logs', project, result), 'psmonitor_output.json')


@app.route('/listProjects')
def list_projects():
    projects = {}
    for project in os.listdir('./logs'):
        projects[project] = []
        for result in os.listdir(join('./logs', project)):
            agg = get_fio_speed(join('./logs', project, result, 'fio.json'))
            projects[project].append({'result':result, 'agg': agg / 1024})
    return projects


@app.route('/listResults/<project>')
def list_results(project):
    return {'results': os.listdir(join('./logs', project))}


app.run(host="0.0.0.0", port="8000")
