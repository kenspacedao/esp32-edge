import json
import logging
import boto3
from datetime import datetime, timezone

# Configure structured CloudWatch logging
logger = logging.getLogger()
logger.setLevel(logging.INFO)

# DynamoDB resource
dynamodb = boto3.resource('dynamodb')
TABLE_NAME = 'ESP32Telemetry'

def lambda_handler(event, context):
    """
    Ingestion handler for ESP32 edge telemetry.
    Triggered by AWS IoT Core Rule on topic: esp32/telemetry
    """
    logger.info(f"Received event: {json.dumps(event)}")

    try:
        # Extract and validate required fields
        device_id  = event.get('device_id')
        temperature = event.get('temperature')
        humidity    = event.get('humidity')
        pm25        = event.get('pm25')
        pm10        = event.get('pm10')
        status      = event.get('status', 'unknown')

        if not device_id:
            raise ValueError("Missing required field: 'device_id'")

        # Generate a server-side ISO timestamp for the record
        timestamp = datetime.now(timezone.utc).isoformat()

        # Build the DynamoDB item
        item = {
            'device_id':   device_id,
            'timestamp':   timestamp,
            'temperature': str(temperature) if temperature is not None else None,
            'humidity':    str(humidity)    if humidity    is not None else None,
            'pm25':        int(pm25)        if pm25        is not None else None,
            'pm10':        int(pm10)        if pm10        is not None else None,
            'status':      status,
        }

        # Remove None values to keep DynamoDB items clean
        item = {k: v for k, v in item.items() if v is not None}

        # Write to DynamoDB
        table = dynamodb.Table(TABLE_NAME)
        table.put_item(Item=item)

        logger.info(f"Successfully wrote telemetry for device '{device_id}' at {timestamp}")
        return {'statusCode': 200, 'body': 'OK'}

    except Exception as e:
        logger.error(f"Failed to process telemetry: {str(e)}", exc_info=True)
        raise
