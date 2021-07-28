from aws_cdk import (
    aws_iam as iam,
    aws_kinesis as kinesis,
    aws_dynamodb as dynamodb,
    aws_lambda_python as aws_lambda_python,
    aws_lambda as _lambda,
    aws_lambda_event_sources as _lambda_event_sources,
    core
)

class EdukitappStack(core.Stack):

    def __init__(self, scope: core.Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        edukitapp_stream = kinesis.Stream(self, "EdukitAgentStream",
            stream_name="edukit-agent-stream"
        )
        
        edukitapp_table = dynamodb.Table(self, "edukit-agent-stream-table",
            partition_key=dynamodb.Attribute(name="AgentARN", type=dynamodb.AttributeType.STRING)
        )

        edukitapp_lambda_role = iam.Role(
            self,"edukitapp_lambda_role",
            assumed_by=iam.ServicePrincipal("lambda.amazonaws.com")
        )
        
        edukitapp_lambda_role.add_to_policy(iam.PolicyStatement(
            resources=["*"],
            actions=[
                "logs:CreateLogGroup",
                "logs:CreateLogStream",
                "logs:PutLogEvents"
            ]
        ))

        edukitapp_lambda_role.add_to_policy(iam.PolicyStatement(
            resources=["*"],
            actions=[
                "iot:UpdateThingShadow"
            ]
        ))


        edukitapp_lambda = aws_lambda_python.PythonFunction(
            self,
            "lambdahandler",
            entry="lambda_stream",
            index="index.py",
            handler="lambda_handler",
            memory_size=512,
            timeout=core.Duration.minutes(1),
            runtime=_lambda.Runtime.PYTHON_3_8,
            role=edukitapp_lambda_role,
            environment={
                'TABLE_NAME': edukitapp_table.table_name
            }
        )

        edukitapp_lambda.add_event_source(_lambda_event_sources.KinesisEventSource(edukitapp_stream,
            starting_position=_lambda.StartingPosition.TRIM_HORIZON
        ))

        edukitapp_table.grant_read_write_data(edukitapp_lambda)


        
