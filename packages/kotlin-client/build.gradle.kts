allprojects {
    group = "com.saulx.based-client"
    version = "0.0.4"

    repositories {
        mavenLocal()
        mavenCentral()
        maven {
            url = uri("https://archiva.devtools.airhub.app/repository/internal/")
            credentials {
                username = System.getenv("AIRHUB_ARCHIVA_USERNAME")
                password = System.getenv("AIRHUB_ARCHIVA_PASSWORD")
            }
        }
    }
}