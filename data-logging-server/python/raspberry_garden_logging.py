import logging
import sys

def GetLogger(name="Raspberry Garden"):
    '''
    Create a new logging object for home alert
    Can give name for logger
    '''
    logger = logging.getLogger(name)
    logger.setLevel(logging.DEBUG)
    stdout_handler = logging.StreamHandler(sys.stdout)
    stdout_handler.setLevel(logging.DEBUG)
    formatter = logging.Formatter('%(name)s : %(asctime)s : %(levelname)s : %(message)s')
    stdout_handler.setFormatter(formatter)
    logger.addHandler(stdout_handler)
    return logger
