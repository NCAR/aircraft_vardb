pipeline {
  agent {
     node { 
        label 'CentOS9_x86_64'
        }
  }
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
      emailext to: "cjw@ucar.edu janine@ucar.edu cdewerd@ucar.edu granger@ucar.edu",
      subject: "Jenkinsfile aircraft_vardb build failed",
      body: "See console output attached",
      attachLog: true
    }
  }
  options {
    buildDiscarder(logRotator(numToKeepStr: '10'))
  }
}
