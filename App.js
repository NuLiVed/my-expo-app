import React, { useState } from 'react';
import { View, Text, StyleSheet, Pressable } from 'react-native';
import SignInModal from './Components/SignInModal';
import SignUpModal from './Components/SignUpModal';
import DashboardScreen from './Components/DashboardScreen';

export default function App() {
  const [showSignIn, setShowSignIn] = useState(false);
  const [showSignUp, setShowSignUp] = useState(false);
  const [loggedIn, setLoggedIn] = useState(false);

  if (loggedIn) {
    return <DashboardScreen onLogout={() => setLoggedIn(false)} />;
  }

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Shelvy</Text>
      <Text style={styles.subtitle}>Monitor your bread's freshness</Text>
      <Text style={styles.subtitle}>🍞</Text>

      <View style={styles.buttonContainer}>
        <Pressable
          style={({ pressed }) => [styles.button, styles.signInButton, pressed && styles.pressed]}
          onPress={() => setShowSignIn(true)}
        >
          <Text style={styles.signInText}>Sign In</Text>
        </Pressable>

        <Pressable
          style={({ pressed }) => [styles.button, styles.signUpButton, pressed && styles.pressed]}
          onPress={() => setShowSignUp(true)}
        >
          <Text style={styles.signUpText}>Sign Up</Text>
        </Pressable>
      </View>

      {/*Sign in*/}
      <SignInModal
        visible={showSignIn}
        onClose={() => setShowSignIn(false)}
        onSignUpPress={() => {
          setShowSignIn(false);
          setShowSignUp(true);
        }}
        onSuccess={() => {
          setShowSignIn(false);
          setLoggedIn(true);
        }}
      />

      {/*Sign up*/}
      <SignUpModal
        visible={showSignUp}
        onClose={() => setShowSignUp(false)}
        onSignInPress={() => {
          setShowSignUp(false);
          setShowSignIn(true);
        }}
        onSuccess={() => {
          setShowSignUp(false);
          setLoggedIn(true);
        }}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f0e6',
    justifyContent: 'center',
    alignItems: 'center',
    paddingHorizontal: 24,
  },
  title: {
    fontSize: 42,
    fontWeight: 'bold',
    color: '#b35c00',
    fontFamily: 'monospace',
    marginBottom: 8,
  },
  subtitle: {
    fontSize: 16,
    color: '#6b3e00',
    fontFamily: 'monospace',
    marginBottom: 10,
    textAlign: 'center',
  },
  buttonContainer: {
    width: '70%',
    gap: 16,
  },
  button: {
    borderRadius: 20,
    paddingVertical: 12,
    alignItems: 'center',
  },
  signInButton: {
    backgroundColor: '#c46a00',
  },
  signUpButton: {
    backgroundColor: '#e0d9d0',
    borderWidth: 1,
    borderColor: '#c46a00',
  },
  pressed: {
    opacity: 0.8,
  },
  signInText: {
    color: 'white',
    fontSize: 18,
    fontWeight: 'bold',
    fontFamily: 'monospace',
  },
  signUpText: {
    color: '#c46a00',
    fontSize: 18,
    fontWeight: 'bold',
    fontFamily: 'monospace',
  },
});
