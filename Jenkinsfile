pipeline {
  agent any
  triggers {
    pollSCM('H/20 7-20 * * *')
  }
  stages {
    stage('Checkout Scm') {
      steps {
        git 'eolJenkins:ncar/aircraft_vardb'
      }
    }
    stage('Build') {
      steps {
        sh 'git submodule update --init --recursive'
        sh 'scons install'
      }
    }
  }
  post {
    failure {
      mail(to: 'cjw@ucar.edu janine@ucar.edu cdewerd@ucar.edu granger@ucar.edu taylort@ucar.edu', subject: 'aircraft_vardb Jenkinsfile build failed', body: 'aircraft_vardb Jenkinsfile build failed')
    }
  }
  options {
    buildDiscarder(logRotator(numToKeepStr: '10'))
  }
}
