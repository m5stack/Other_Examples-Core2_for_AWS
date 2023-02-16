#!/usr/bin/env python3

from aws_cdk import core

from kitapp.kitapp_stack import KitappStack

env_LON = core.Environment(region="eu-west-2")

app = core.App()
KitappStack(app, "kitapp", env=env_LON)

app.synth()
