import os
from flask import Flask, Response, request
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
  
        # Add endpoints
        self.add_endpoint(endpoint='/',
                endpoint_name='index', handler=self.index)
        self.add_endpoint(endpoint='/status',
                endpoint_name='status', handler=self.status)
        self.add_endpoint(endpoint='/update',
                endpoint_name='update', handler=self.update, methods=['POST'])


    def index(self):
        '''
        Index page, returns status page.
        '''
        return self.status()


    def status(self):
        '''
        Returns node location and arm status:
        '''
        return 'TODO'


    def update(self):
        dt = datetime.datetime.now(pytz.timezone('America/Los_Angeles'))
        ymlStr = request.data.decode('utf-8')
        yml = yaml.load(ymlStr);
        ymlFileName = ymlDir + yml['location'] + '_' + str(dt.date()) + '.yml';
        with open(ymlFileName, 'w') as f:
            f.write(ymlStr);
        print(yml)
        return ''


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
    server.run(5000)


if __name__ == '__main__':
    main()
