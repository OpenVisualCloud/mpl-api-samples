# Doxygen Guide for APIs
## 1.Install the doxygen tools
```shell
sudo apt-get install doxygen
```
## 2.Build
Just run below command in the top tree of the IMPL project, then check doxygen/html/index.html for the doxygen documents.
```shell
cd doc/doxygen
rm html -rf
source build-doc.sh
```
