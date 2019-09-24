#!/bin/sh

python3 -m venv venv
source venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install paho-mqtt
python3 -m pip install pandas
python3 -m pip install openpyxl

python3 -m pip install jupyterlab
python3 -m pip install bokeh
python3 -m pip install ipywidgets
python3 -m pip install jupyter_nbextensions_configurator

jupyter labextension install jupyterlab_bokeh
jupyter labextension install @jupyterlab/celltags
jupyter labextension install @jupyter-widgets/jupyterlab-manager

