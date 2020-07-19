import os
from flask import Flask, Response, request, render_template
import datetime
import pytz
import threading
import yaml

import raspberry_garden_logging
import pprint

from bokeh.models import (HoverTool, FactorRange, Plot, LinearAxis, Grid,
                          Range1d)
from bokeh.models.glyphs import VBar, Line, Circle
from bokeh.plotting import figure
from bokeh.embed import components
from bokeh.models.sources import ColumnDataSource

ymlDir = os.path.expanduser('~/.raspberry-garden-data/')

def create_hover_tool(key):
    return HoverTool(tooltips=[('date', '@date_str'), (key, '@' + key)])


def create_bar_chart(data, title, x_name, y_name, hover_tool=None,
                     width=1200, height=300, days=None):
    """Creates a bar chart plot with the exact styling for the centcom
       dashboard. Pass in data as a dictionary, desired plot title,
       name of x axis, y axis and the hover tool HTML.
    """
    source = ColumnDataSource(data)
    date_end = max(data[x_name])
    if days is None:
        date_start = min(data[x_name])
    else:
        date_start = date_end - datetime.timedelta(days=days)
    xdr = Range1d(start=date_start, end=date_end)
    ydr = Range1d(start=min(data[y_name]),end=max(data[y_name]))

    tools = []
    if hover_tool:
        tools = [hover_tool,]

    plot = figure(title=title, x_range=xdr, y_range=ydr, plot_width=width,
                  plot_height=height, min_border=0, toolbar_location="above", tools=tools,
                  sizing_mode='scale_width', outline_line_color="#666666", x_axis_type='datetime')

    line_glyph = Line(x=x_name, y=y_name, line_width=2, line_color="#e12127")
    circle_glyph = Circle(x=x_name, y=y_name, size=3, line_width=2, line_color="#e12127", fill_color='#e12127')
    plot.add_glyph(source, line_glyph)
    plot.add_glyph(source, circle_glyph)

    xaxis = LinearAxis()
    yaxis = LinearAxis()

    plot.add_layout(Grid(dimension=0, ticker=xaxis.ticker))
    plot.add_layout(Grid(dimension=1, ticker=yaxis.ticker))
    plot.toolbar.logo = None
    plot.min_border_top = 0
    plot.xgrid.grid_line_color = None
    plot.ygrid.grid_line_color = "#999999"
    plot.yaxis.axis_label = y_name
    plot.ygrid.grid_line_alpha = 0.1
    plot.xaxis.axis_label = x_name 
    plot.xaxis.major_label_orientation = 1
    return plot


class EndpointAction:
    '''
    action is expected to return a valid response
    '''
    def __init__(self, action):
        self.action = action


    def __call__(self, *args):
        return self.action()

class LoggingMiddleware(object):
    def __init__(self, app):
        self._app = app

    def __call__(self, environ, resp):
        errorlog = environ['wsgi.errors']
        pprint.pprint(('REQUEST', environ), stream=errorlog)

        def log_response(status, headers, *args):
            pprint.pprint(('RESPONSE', status, headers), stream=errorlog)
            return resp(status, headers, *args)

        return self._app(environ, log_response)

class RaspberryGardenWebServer:
    '''
    Class holding a flask app and smtp server
    '''
    def __init__(self, http_logging = False, s3_bucket = None):
        '''
        '''
        self.logger = raspberry_garden_logging.GetLogger()

        self.logger.info("Connecting to mail server")
        self.logger.info("Creating Web Server")
        self.app = Flask('Raspberry Garden Data Logging Server')
        if http_logging:
            self.app.wsgi_app = LoggingMiddleware(self.app.wsgi_app)

        if not os.path.exists(ymlDir):
            os.makedirs(ymlDir)

        ''' Map of location name to list of data points '''
        self.data = {}
        ''' Map of sensor ids to location '''
        self.sensors = {}
        self.reloadData()
        self.reloadSensors()
  
        # Add endpoints
        self.add_endpoint(endpoint='/',
                endpoint_name='index', handler=self.index)
        self.add_endpoint(endpoint='/status',
                endpoint_name='status', handler=self.status, methods=['GET'])
        self.add_endpoint(endpoint='/update',
                endpoint_name='update', handler=self.update, methods=['POST'])
        self.add_endpoint(endpoint='/locations',
                endpoint_name='locations', handler=self.locations)
        self.add_endpoint(endpoint='/history',
                endpoint_name='history', handler=self.history)
        self.add_endpoint(endpoint='/chart',
                endpoint_name='chart', handler=self.chart)
        self.add_endpoint(endpoint='/reload',
                endpoint_name='reload', handler=self.reload)

    def index(self):
        '''
        Index page, returns status page.
        '''
        return self.status()


    def history(self):
        ''' returns a list of historical values for location and keys, most recent first with date of values'''
        location = request.args.get('location')
        key = request.args.get('key')
        vals = []
        for data_entry in reversed(self.data[location]):
            if key in data_entry and 'date' in data_entry:
                vals.append((data_entry['date'], data_entry[key]))
        return render_template('history.html', location=location, key=key, vals=vals)


    def chart(self):
        ''' returns a graph of values for location and given key, ordered by date'''
        location = request.args.get('location')
        days = request.args.get('days')
        if days == '':
            days = None
        if days is not None:
            days=int(days)
        key = request.args.get('key')
        vals = {'date': [], key: []}
        for data_entry in reversed(self.data[location]):
            if key in data_entry and 'date' in data_entry:
                if data_entry[key] != 'nan':
                    vals['date'].append(data_entry['date'])
                    vals[key].append(data_entry[key])
        vals['date_str'] = [str(d) for d in vals['date']]
        plot = create_bar_chart(vals, location, 'date', key, hover_tool=create_hover_tool(key), days=days)
        plot_script, plot_div = components(plot)
        return render_template('chart.html', location=location, key=key, vals=vals, plot_script=plot_script, plot_div=plot_div, days=days)


    def status(self):
        '''
        Returns node location and arm status:
        '''
        location = request.args.get('location')
        toCheck = [location] 
        if location is None:
            toCheck = sorted(self.data.keys())

        locations = []
        for l in toCheck:
            locations.append(self.data[l][-1])
                        
        return render_template('status.html', locations=locations)


    def locations(self):
        return render_template('locations.html', locations=sorted(self.data.keys()))


    def reloadSensors(self):
        with open('./sensors.yml', 'r') as f:
            self.sensors = yaml.load(f.read())
            print(self.sensors)


    def reloadData(self):
        self.data = {}
        for path in os.listdir(ymlDir):
            full_path = os.path.join(ymlDir, path)
            if os.path.isdir(full_path):
                self.data[path] = []
                for update in sorted(os.listdir(full_path)):
                    update_path = os.path.join(full_path, update)
                    print('Loading {}'.format(update_path))
                    with open(update_path, 'r') as f:
                        yml = yaml.load(f.read())
                        self.data[path].append(yml)


    def reload(self):
        self.reloadData()
        self.reloadSensors()
        return "Reloaded data and sensor locations"
 

    def update(self):
        ymlStr = request.data.decode('utf-8')
        yml = yaml.load(ymlStr)
        location = self.sensors[yml['sensor-id']]
        yml['location'] = location
        dt = yml['date']

        # Save it to a file
        locationDir = ymlDir + location + '/'
        if not os.path.exists(locationDir):
            os.makedirs(locationDir)
        ymlFileName = locationDir + location + '_' + str(dt.date()) + '_' + str(dt.time()) + '.yml'
        with open(ymlFileName, 'w') as f:
            f.write(ymlStr)

        # Append to data
        if location not in self.data:
            self.data[location] = []
        self.data[location].append(yml)

        print(yml)
        return 'Update Received'


    def run(self, port):
        '''
        Run the flask app
        '''
        self.app.run(debug=False, host='0.0.0.0', port=port)


    def add_endpoint(self, endpoint=None, endpoint_name=None, handler=None, **options):
        '''
        Register an endpoint function to the flask app
        '''
        self.app.add_url_rule(endpoint, endpoint_name, EndpointAction(handler), **options)

def main():
    server = RaspberryGardenWebServer()
    server.run(5050)


if __name__ == '__main__':
    main()
