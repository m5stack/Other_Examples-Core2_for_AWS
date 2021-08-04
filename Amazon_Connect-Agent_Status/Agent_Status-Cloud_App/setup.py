import setuptools


with open("README.md") as fp:
    long_description = fp.read()


setuptools.setup(
    name="edukitapp",
    version="0.0.1",

    description="A sample CDK Python app",
    long_description=long_description,
    long_description_content_type="text/markdown",

    author="author",

    package_dir={"": "edukitapp"},
    packages=setuptools.find_packages(where="edukitapp"),

    install_requires=[
        "aws-cdk.core==1.110.1",
        "aws-cdk.aws_iam==1.110.1",
        "aws-cdk.aws_kinesis==1.110.1",
        "aws-cdk.aws_dynamodb==1.110.1",
        "aws-cdk.aws-lambda-python==1.110.1",
        "aws-cdk.aws-lambda-event-sources==1.110.1"
    ],

    python_requires=">=3.6",

    classifiers=[
        "Development Status :: 4 - Beta",

        "Intended Audience :: Developers",

        "Programming Language :: JavaScript",
        "Programming Language :: Python :: 3 :: Only",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",

        "Topic :: Software Development :: Code Generators",
        "Topic :: Utilities",

        "Typing :: Typed",
    ],
)
