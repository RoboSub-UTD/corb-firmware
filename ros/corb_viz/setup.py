from glob import glob

from setuptools import find_packages, setup

package_name = "corb_viz"

setup(
    name=package_name,
    version="0.1.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
        ("share/" + package_name + "/launch", glob("launch/*.py")),
        ("share/" + package_name + "/config", glob("config/*")),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Alec Jensen",
    maintainer_email="alecbradyjensen@gmail.com",
    description="Visualization and link monitoring for the CORB VCU",
    license="Proprietary",
    entry_points={
        "console_scripts": [
            "viz_node = corb_viz.viz_node:main",
            "test_commander = corb_viz.test_commander:main",
        ],
    },
)
