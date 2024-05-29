FROM quay.io/pypa/manylinux_2_28_x86_64
ARG QT_VERSION=6.7.1
ENV QT_VERSION=${QT_VERSION}
ENV PATH="/opt/Qt/${QT_VERSION}/gcc_64/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/Qt/${QT_VERSION}/gcc_64/lib:${LD_LIBRARY_PATH}"
ENV PATH="/opt/python/cp310-cp310/bin:${PATH}"

RUN pip install --upgrade pip setuptools meson ninja meson-python wheel shiboken6==${QT_VERSION} \
    pyside6==${QT_VERSION} shiboken6_generator==${QT_VERSION} aqtinstall &&\
    aqt install-qt -O "/opt/Qt" linux desktop ${QT_VERSION}
