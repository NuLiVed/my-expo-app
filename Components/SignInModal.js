import React, { useState } from 'react';
import {
  View,
  Text,
  Modal,
  Pressable,
  StyleSheet,
  ScrollView,
} from 'react-native';
import InputField from './InputField';
import ButtonPrimary from './ButtonPrimary';

export default function SignInModal({ visible, onClose, onSignUpPress }) {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [rememberMe, setRememberMe] = useState(false);

  const handleSignIn = () => {
    console.log('Signed in with:', { email, password, rememberMe });
    onClose();
  };

  return (
    <Modal visible={visible} animationType="slide" transparent={true}>
      <View style={styles.overlay}>
        <View style={styles.container}>
          <Text style={styles.title}>Shelvy</Text>
          <Text style={styles.welcome}>Welcome back!</Text>

          <ScrollView
            keyboardShouldPersistTaps="handled"
            contentContainerStyle={{ paddingBottom: 20 }}
          >
            <InputField
              placeholder="📧 Email Address"
              value={email}
              onChangeText={setEmail}
            />
            <InputField
              placeholder="🔒 Password"
              value={password}
              onChangeText={setPassword}
              secureTextEntry
            />

            {/* Remember Me */}
            <Pressable
              onPress={() => setRememberMe(!rememberMe)}
              style={styles.checkboxRow}
            >
              <View style={[styles.checkbox, rememberMe && styles.checked]} />
              <Text style={styles.checkboxLabel}>Remember me</Text>
            </Pressable>

            <ButtonPrimary label="Sign In" onPress={handleSignIn} />

            {/* New to Shelvy */}
            <Pressable style={styles.createAccount} onPress={onSignUpPress}>
              <Text style={styles.createText}>
                New to Shelvy?{' '}
                <Text style={styles.createLink}>Create an account</Text>
              </Text>
            </Pressable>

            <Pressable style={styles.closeButton} onPress={onClose}>
              <Text style={styles.closeText}>Close</Text>
            </Pressable>
          </ScrollView>
        </View>
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  overlay: {
    flex: 1,
    backgroundColor: 'rgba(245, 240, 230, 0.95)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  container: {
    backgroundColor: '#f5f0e6',
    borderRadius: 20,
    padding: 24,
    width: 320,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.3,
    shadowRadius: 6,
    elevation: 10,
  },
  title: {
    fontSize: 34,
    fontWeight: 'bold',
    color: '#b35c00',
    textAlign: 'center',
    fontFamily: 'monospace',
  },
  welcome: {
    fontSize: 18,
    color: '#6b3e00',
    textAlign: 'center',
    marginBottom: 24,
    fontFamily: 'monospace',
  },
  checkboxRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 20,
  },
  checkbox: {
    width: 18,
    height: 18,
    borderWidth: 1.5,
    borderColor: '#c46a00',
    borderRadius: 4,
    marginRight: 8,
  },
  checked: {
    backgroundColor: '#c46a00',
  },
  checkboxLabel: {
    fontSize: 14,
    color: '#6b3e00',
    fontFamily: 'monospace',
  },
  createAccount: {
    alignItems: 'center',
    marginTop: 12,
  },
  createText: {
    color: '#6b3e00',
    fontSize: 14,
    fontFamily: 'monospace',
  },
  createLink: {
    color: '#c46a00',
    fontWeight: 'bold',
    textDecorationLine: 'underline',
  },
  closeButton: {
    alignSelf: 'center',
    marginTop: 12,
  },
  closeText: {
    color: '#c46a00',
    fontWeight: 'bold',
    fontSize: 16,
    fontFamily: 'monospace',
  },
});
