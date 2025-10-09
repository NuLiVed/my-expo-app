import React, { useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  Pressable,
  Modal,
  TextInput,
  ScrollView,
  TouchableOpacity,
} from 'react-native';

export default function Shelvy() {
  const [showLogin, setShowLogin] = useState(false);
  const [showSignup, setShowSignup] = useState(false);
  const [loggedIn, setLoggedIn] = useState(false);

  // Login form state
  const [loginEmail, setLoginEmail] = useState('');
  const [loginPassword, setLoginPassword] = useState('');
  const [rememberMe, setRememberMe] = useState(false);

  // Signup form state
  const [signupName, setSignupName] = useState('');
  const [signupEmail, setSignupEmail] = useState('');
  const [signupPassword, setSignupPassword] = useState('');
  const [agreeTerms, setAgreeTerms] = useState(false);

  const handleLogin = () => {
    // For now, just close modal and show dashboard
    setShowLogin(false);
    setLoggedIn(true);
  };

  const handleSignup = () => {
    // For now, just close modal and show dashboard
    setShowSignup(false);
    setLoggedIn(true);
  };

  if (loggedIn) {
    return (
      <View style={styles.dashboardContainer}>
        <Text style={styles.dashboardTitle}>Dashboard</Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Shelvy</Text>
      <View style={styles.buttonRow}>
        <Pressable
          style={({ pressed }) => [
            styles.button,
            styles.loginButton,
            pressed && styles.buttonPressed,
          ]}
          onPress={() => setShowLogin(true)}
        >
          <Text style={styles.loginButtonText}>Log In</Text>
        </Pressable>
        <Pressable
          style={({ pressed }) => [
            styles.button,
            styles.signupButton,
            pressed && styles.buttonPressed,
          ]}
          onPress={() => setShowSignup(true)}
        >
          <Text style={styles.signupButtonText}>Sign Up</Text>
        </Pressable>
      </View>

      {/* Login Modal */}
      <Modal
        visible={showLogin}
        animationType="slide"
        transparent={true}
        onRequestClose={() => setShowLogin(false)}
      >
        <View style={styles.modalOverlay}>
          <View style={styles.modalBox}>
            <Text style={styles.modalTitle}>Welcome back!</Text>
            <Text style={styles.modalSubtitle}>
              Sign in to monitor your bread quality
            </Text>

            <ScrollView style={{ width: '100%' }} keyboardShouldPersistTaps="handled">
              <Text style={styles.inputLabel}>📧 Email Address</Text>
              <TextInput
                style={styles.input}
                placeholder="Enter your email address"
                keyboardType="email-address"
                autoCapitalize="none"
                value={loginEmail}
                onChangeText={setLoginEmail}
              />

              <Text style={styles.inputLabel}>🔒 Password</Text>
              <TextInput
                style={styles.input}
                placeholder="Enter your password"
                secureTextEntry={true}
                value={loginPassword}
                onChangeText={setLoginPassword}
              />

              <View style={styles.rowBetween}>
                <Pressable
                  onPress={() => setRememberMe(!rememberMe)}
                  style={styles.checkboxContainer}
                >
                  <View style={[styles.checkbox, rememberMe && styles.checkboxChecked]} />
                  <Text style={styles.checkboxLabel}>Remember me</Text>
                </Pressable>
                <Pressable>
                  <Text style={styles.forgotPassword}>Forgot password?</Text>
                </Pressable>
              </View>

              <Pressable
                style={styles.signInButton}
                onPress={handleLogin}
              >
                <Text style={styles.signInButtonText}>🔑 Sign In</Text>
              </Pressable>

              <View style={styles.orSeparator}>
                <View style={styles.line} />
                <Text style={styles.orText}>or</Text>
                <View style={styles.line} />
              </View>

              <Pressable
                onPress={() => {
                  setShowLogin(false);
                  setShowSignup(true);
                }}
                style={styles.createAccountContainer}
              >
                <Text style={styles.createAccountText}>
                  New to Shelvy? <Text style={styles.createAccountLink}>Create an account</Text>
                </Text>
              </Pressable>
            </ScrollView>

            <Pressable
              style={styles.closeButton}
              onPress={() => setShowLogin(false)}
            >
              <Text style={styles.closeButtonText}>Close</Text>
            </Pressable>
          </View>
        </View>
      </Modal>

      {/* Signup Modal */}
      <Modal
        visible={showSignup}
        animationType="slide"
        transparent={true}
        onRequestClose={() => setShowSignup(false)}
      >
        <View style={styles.modalOverlay}>
          <View style={styles.modalBox}>
            <Text style={styles.modalTitle}>Join the bakery!</Text>
            <Text style={styles.modalSubtitle}>
              Start monitoring your bread quality today
            </Text>

            <ScrollView style={{ width: '100%' }} keyboardShouldPersistTaps="handled">
              <Text style={styles.inputLabel}>👤 Full Name</Text>
              <TextInput
                style={styles.input}
                placeholder="Enter your full name"
                value={signupName}
                onChangeText={setSignupName}
              />

              <Text style={styles.inputLabel}>📧 Email Address</Text>
              <TextInput
                style={styles.input}
                placeholder="Enter your email address"
                keyboardType="email-address"
                autoCapitalize="none"
                value={signupEmail}
                onChangeText={setSignupEmail}
              />

              <Text style={styles.inputLabel}>🔒 Password</Text>
              <TextInput
                style={styles.input}
                placeholder="Create a strong password"
                secureTextEntry={true}
                value={signupPassword}
                onChangeText={setSignupPassword}
              />

              <View style={styles.termsContainer}>
                <Text style={styles.termsText}>
                  📋 By joining our bakery family, I agree to the{' '}
                  <Text style={styles.link}>Terms of Service</Text> and{' '}
                  <Text style={styles.link}>Privacy Policy</Text>
                </Text>
                <Pressable
                  onPress={() => setAgreeTerms(!agreeTerms)}
                  style={styles.checkboxContainer}
                >
                  <View style={[styles.checkbox, agreeTerms && styles.checkboxChecked]} />
                </Pressable>
              </View>

              <Pressable
                style={styles.signUpButton}
                onPress={handleSignup}
                disabled={!agreeTerms}
              >
                <Text style={styles.signUpButtonText}>✨ Sign Up</Text>
              </Pressable>
            </ScrollView>

            <Pressable
              style={styles.closeButton}
              onPress={() => setShowSignup(false)}
            >
              <Text style={styles.closeButtonText}>Close</Text>
            </Pressable>
          </View>
        </View>
      </Modal>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f0e6',
    alignItems: 'center',
    justifyContent: 'flex-start',
    paddingTop: 80,
    paddingHorizontal: 20,
  },
  title: {
    fontSize: 36,
    fontWeight: 'bold',
    marginBottom: 40,
    color: '#b35c00',
    fontFamily: 'monospace',
  },
  buttonRow: {
    flexDirection: 'row',
    width: '60%',
    justifyContent: 'space-between',
  },
  button: {
    paddingVertical: 10,
    paddingHorizontal: 20,
    borderRadius: 20,
    minWidth: 100,
    alignItems: 'center',
  },
  loginButton: {
    backgroundColor: '#c46a00',
  },
  signupButton: {
    backgroundColor: '#e0d9d0',
  },
  loginButtonText: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 16,
  },
  signupButtonText: {
    color: '#c46a00',
    fontWeight: 'bold',
    fontSize: 16,
  },
  buttonPressed: {
    opacity: 0.7,
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(245, 240, 230, 0.9)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  modalBox: {
    backgroundColor: '#f5f0e6',
    borderRadius: 20,
    padding: 24,
    width: 320,
    maxHeight: '90%',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.3,
    shadowRadius: 6,
    elevation: 10,
  },
  modalTitle: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#6b3e00',
    marginBottom: 6,
    textAlign: 'center',
    fontFamily: 'monospace',
  },
  modalSubtitle: {
    fontSize: 14,
    color: '#a67c00',
    marginBottom: 20,
    textAlign: 'center',
    fontFamily: 'monospace',
  },
  inputLabel: {
    fontSize: 14,
    color: '#6b3e00',
    marginBottom: 6,
    fontFamily: 'monospace',
  },
  input: {
    borderWidth: 1,
    borderColor: '#d9b87a',
    borderRadius: 12,
    paddingHorizontal: 12,
    paddingVertical: 10,
    marginBottom: 16,
    fontSize: 16,
    fontFamily: 'monospace',
    color: '#6b3e00',
    backgroundColor: 'white',
  },
  rowBetween: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  checkboxContainer: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  checkbox: {
    width: 18,
    height: 18,
    borderWidth: 1.5,
    borderColor: '#c46a00',
    borderRadius: 4,
    marginRight: 8,
  },
  checkboxChecked: {
    backgroundColor: '#c46a00',
  },
  checkboxLabel: {
    fontSize: 14,
    color: '#6b3e00',
    fontFamily: 'monospace',
  },
  forgotPassword: {
    fontSize: 14,
    color: '#c46a00',
    textDecorationLine: 'underline',
    fontFamily: 'monospace',
  },
  signInButton: {
    backgroundColor: '#c46a00',
    borderRadius: 20,
    paddingVertical: 12,
    alignItems: 'center',
    marginBottom: 12,
  },
  signInButtonText: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 18,
    fontFamily: 'monospace',
  },
  orSeparator: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 12,
  },
  line: {
    flex: 1,
    height: 1,
    backgroundColor: '#d9b87a',
  },
  orText: {
    marginHorizontal: 8,
    color: '#a67c00',
    fontFamily: 'monospace',
  },
  createAccountContainer: {
    alignItems: 'center',
    marginBottom: 12,
  },
  createAccountText: {
    color: '#6b3e00',
    fontSize: 14,
    fontFamily: 'monospace',
  },
  createAccountLink: {
    color: '#c46a00',
    fontWeight: 'bold',
    textDecorationLine: 'underline',
  },
  closeButton: {
    alignSelf: 'center',
    marginTop: 8,
  },
  closeButtonText: {
    color: '#c46a00',
    fontSize: 16,
    fontWeight: 'bold',
    fontFamily: 'monospace',
  },
  termsContainer: {
    backgroundColor: '#f9f4e8',
    borderRadius: 12,
    padding: 12,
    marginBottom: 16,
    flexDirection: 'row',
    alignItems: 'center',
  },
  termsText: {
    flex: 1,
    fontSize: 12,
    color: '#6b3e00',
    fontFamily: 'monospace',
  },
  link: {
    color: '#c46a00',
    textDecorationLine: 'underline',
  },
  signUpButton: {
    backgroundColor: '#c46a00',
    borderRadius: 20,
    paddingVertical: 12,
    alignItems: 'center',
  },
  signUpButtonText: {
    color: 'white',
    fontWeight: 'bold',
    fontSize: 18,
    fontFamily: 'monospace',
  },
  dashboardContainer: {
    flex: 1,
    backgroundColor: '#f5f0e6',
    justifyContent: 'center',
    alignItems: 'center',
  },
  dashboardTitle: {
    fontSize: 32,
    fontWeight: 'bold',
    color: '#b35c00',
    fontFamily: 'monospace',
  },
});
