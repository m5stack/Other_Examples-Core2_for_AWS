#!/usr/bin/env python3

from aws_cdk import core

from edukitapp.edukitapp_stack import EdukitappStack

env_LON = core.Environment(region="eu-west-2")

app = core.App()
EdukitappStack(app, "edukitapp", env=env_LON)

app.synth()
