pipeline {
  agent {
     node {
        label 'CentOS8'
        }
  }
  triggers {
  pollSCM('H/15 7-20 * * *')
  }
  stages {
    stage('Build') {
      steps {
        sh 'scons'
      }
    }
  }
  post {
    success {
      mail(to: 'cjw@ucar.edu janine@ucar.edu cdewerd@ucar.edu taylort@ucar.edu', subject: 'aircraft_vardb Jenkinsfile build successful', body: 'aircraft_vardb Jenkinsfile build successful')
    }
  }
  options {
    buildDiscarder(logRotator(numToKeepStr: '10'))
  }
}
