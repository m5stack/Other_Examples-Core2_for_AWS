import json
import boto3
import base64
import os

TABLE_NAME = os.environ['TABLE_NAME']

dynamodb_client = boto3.client('dynamodb')
iotdata_client = boto3.client('iot-data', region_name='eu-west-1')


def get_record(AgentARN):
    try:
        response = dynamodb_client.query(
            TableName=TABLE_NAME,
            KeyConditionExpression='AgentARN = :AgentARN',
            ExpressionAttributeValues={':AgentARN': {'S': AgentARN } }
        )
    except:
        return 1,'dynamodb_client'
    return 0,response


def publish_shadow(device,record):
    
    if record['CurrentAgentSnapshot']['AgentStatus']['Name'] == "Available":
        record['CurrentAgentSnapshot']['AgentStatus']['Name'] = 'AVAILABLE'
    elif record['CurrentAgentSnapshot']['AgentStatus']['Name']  == "Offline":
        record['CurrentAgentSnapshot']['AgentStatus']['Name'] = 'OFFLINE00'
    
    try:
        response = iotdata_client.update_thing_shadow(
            thingName=device,
            payload = json.dumps({
                'state': { 'desired':  record['CurrentAgentSnapshot']['AgentStatus']  }
            })
        )
    except:
        return 1,'publish_shadow'
    return 0,response


def lambda_handler(event, context):
    #print(json.dumps(event))
    for record in event['Records']:

        record = json.loads(base64.b64decode(record['kinesis']['data']).decode('ascii'))
        
        error,response = get_record(record['AgentARN'])
        if error > 0:
            return { 'status' : 'ERROR:'+response }
        
        error,response = publish_shadow(response['Items'][0]['client_id']['S'],record)
        if error > 0:
            return { 'status' : 'ERROR:'+response }
        
        #print(response['Items'][0]['client_id']['S'])
        print(record['CurrentAgentSnapshot']['AgentStatus']['Name'])
        

    return { 'status' : 'ok' }
