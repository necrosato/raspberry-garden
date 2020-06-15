import os
from flask import Flask, Response, request, render_template
import datetime
import pytz
import threading
import yaml

import raspberry_garden_logging
import pprint

ymlDir = os.path.expanduser('~/.raspberry-garden-data/')


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

        self.reloadData()
  
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

    def index(self):
        '''
        Index page, returns status page.
        '''
        return self.status()


    def history(self):
        ''' returns a list of historical values for location and given keys, most recent first '''
        location = request.args.get('location')
        key = request.args.get('key')
        vals = []
        for data_entry in reversed(self.data[location]):
            if key in data_entry and 'date' in data_entry:
                vals.append((data_entry['date'], data_entry[key]))
        return render_template('history.html', location=location, key=key, vals=vals)


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
 

    def update(self):
        ymlStr = request.data.decode('utf-8')
        yml = yaml.load(ymlStr);
        location = yml['location']
        dt = yml['date']

        # Save it to a file
        locationDir = ymlDir + location + '/';
        if not os.path.exists(locationDir):
            os.makedirs(locationDir)
        ymlFileName = locationDir + location + '_' + str(dt.date()) + '_' + str(dt.time()) + '.yml';
        with open(ymlFileName, 'w') as f:
            f.write(ymlStr);

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