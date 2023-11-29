FROM ubuntu:focal

ENV DEBIAN_FRONTEND noninteractive

RUN echo "tzdata tzdata/Areas select Europe" | debconf-set-selections
RUN echo "tzdata tzdata/Zones/Europe select Madrid" | debconf-set-selections
RUN apt-get update \
    && apt-get install -y apt-utils gnupg wget \
    && wget -qO - http://archive.neon.kde.org/public.key | apt-key add - \
    && echo "deb http://archive.neon.kde.org/user focal main" >> /etc/apt/sources.list \
    && echo "deb-src http://archive.neon.kde.org/user focal main" >> /etc/apt/sources.list \
    && sed -i -- 's/#[ ]*deb-src/deb-src/g' /etc/apt/sources.list \
    && apt-get update \
    && apt-get install -y eatmydata \
    && eatmydata apt-get upgrade -y \
    && eatmydata apt-get install -y openssh-server rsync iputils-ping telnet \
    && eatmydata apt-get build-dep -y kmymoney \
    && eatmydata apt-get install -y qt5-default \
    && eatmydata apt-get install -y libsqlcipher-dev \
    && eatmydata apt-get install -y libqt5sql5-* \
    && eatmydata apt-get install -y subversion python3-dev ninja-build \
    && eatmydata apt-get install -y breeze-icon-theme fonts-noto-color-emoji

RUN mkdir /var/run/sshd \
    && echo 'root:root' | chpasswd \
    && sed -i 's/#\?PermitRootLogin .*/PermitRootLogin yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

# 22 for ssh server. 
EXPOSE 22 

########################################################
# Add custom packages and development environment here
########################################################

# ARG USERNAME=Ariel
# ARG USER_UID=1000
# ARG USER_GID=$USER_UID

# # Create the user
# RUN groupadd --gid $USER_GID $USERNAME \
#     && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
#     #
#     # [Optional] Add sudo support. Omit if you don't need to install software after connecting.
#     && apt-get install -y sudo \
#     && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
#     && chmod 0440 /etc/sudoers.d/$USERNAME

########################################################

CMD ["/usr/sbin/sshd", "-D"]
