add_subdirectory(app)
add_subdirectory(core)
add_subdirectory(widgets)
add_subdirectory(resources)

set(HTODO_SOURCES
    ${HTODO_APP_SOURCES}
    ${HTODO_CORE_SOURCES}
    ${HTODO_WIDGET_SOURCES}
    ${HTODO_RESOURCE_SOURCES}
    PARENT_SCOPE
)
