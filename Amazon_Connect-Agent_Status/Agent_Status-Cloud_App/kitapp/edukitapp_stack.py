from aws_cdk import (
    aws_iam as iam,
    aws_kms as kms,
    aws_kinesis as kinesis,
    aws_dynamodb as dynamodb,
    aws_lambda_python as aws_lambda_python,
    aws_lambda as _lambda,
    aws_lambda_event_sources as _lambda_event_sources,
    core
)

class KitappStack(core.Stack):

    def __init__(self, scope: core.Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        kitapp_stream = kinesis.Stream(self, "KitAgentStream",
            stream_name="kit-agent-stream",
            encryption=kinesis.StreamEncryption.KMS
        )
        
        kitapp_table = dynamodb.Table(self, "kit-agent-stream-table",
            partition_key=dynamodb.Attribute(name="AgentARN", type=dynamodb.AttributeType.STRING)
        )

        kitapp_lambda_role = iam.Role(
            self,"kitapp_lambda_role",
            assumed_by=iam.ServicePrincipal("lambda.amazonaws.com")
        )
        
        kitapp_lambda_role.add_to_policy(iam.PolicyStatement(
            resources=["*"],
            actions=[
                "logs:CreateLogGroup",
                "logs:CreateLogStream",
                "logs:PutLogEvents"
            ]
        ))

        kitapp_lambda_role.add_to_policy(iam.PolicyStatement(
            resources=["*"],
            actions=[
                "iot:UpdateThingShadow"
            ]
        ))


        kitapp_lambda = aws_lambda_python.PythonFunction(
            self,
            "lambdahandler",
            entry="lambda_stream",
            index="index.py",
            handler="lambda_handler",
            memory_size=512,
            timeout=core.Duration.minutes(1),
            runtime=_lambda.Runtime.PYTHON_3_8,
            role=kitapp_lambda_role,
            environment={
                'TABLE_NAME': kitapp_table.table_name
            }
        )

        kitapp_lambda.add_event_source(_lambda_event_sources.KinesisEventSource(kitapp_stream,
            starting_position=_lambda.StartingPosition.TRIM_HORIZON
        ))

        kitapp_table.grant_read_write_data(kitapp_lambda)


        
