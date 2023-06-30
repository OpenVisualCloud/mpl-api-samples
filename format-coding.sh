set -e

find . -regex '.*\.\(cpp\|hpp\)' -exec clang-format -i {} \;
