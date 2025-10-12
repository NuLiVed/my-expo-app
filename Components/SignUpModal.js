import React, { useState } from 'react';
import {
  View,
  Text,
  Modal,
  Pressable,
  StyleSheet,
  ScrollView,
} from 'react-native';
import { MaterialCommunityIcons } from '@expo/vector-icons';
import InputField from './InputField';
import ButtonPrimary from './ButtonPrimary';

export default function SignUpModal({ visible, onClose, onSignInPress, onSuccess }) {
  const [name, setName] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [agreeTerms, setAgreeTerms] = useState(false);

  const handleSignUp = () => {
    if (agreeTerms) {
      console.log('Signed up with:', { name, email, password });
      onSuccess();
    } else {
      alert('Please agree to the terms.');
    }
  };

  return (
    <Modal visible={visible} animationType="slide" transparent={true}>
      <View style={styles.overlay}>
        <View style={styles.container}>
          <Text style={styles.title}>Shelvy</Text>
          <Text style={styles.welcome}>Join the bakery!</Text>

          <ScrollView
            keyboardShouldPersistTaps="handled"
            contentContainerStyle={{ paddingBottom: 20 }}
          >
            <InputField
              placeholder="👤 Full Name"
              value={name}
              onChangeText={setName}
            />
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

            {/* Terms Agreement */}
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
                <View style={[styles.checkbox, agreeTerms && styles.checked]} />
              </Pressable>
            </View>

            <ButtonPrimary label="Sign Up" onPress={handleSignUp} />

            {/* Already have account */}
            <Pressable style={styles.signInAccount} onPress={onSignInPress}>
              <Text style={styles.signInText}>
                Already have an account?{' '}
                <Text style={styles.signInLink}>Sign In</Text>
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
    backgroundColor: 'rgba(245, 245, 245, 0.95)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  container: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
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
    color: '#A0522D',
    textAlign: 'center',
    fontFamily: 'System',
  },
  welcome: {
    fontSize: 18,
    color: '#A0522D',
    textAlign: 'center',
    marginBottom: 24,
    fontFamily: 'System',
  },
  termsContainer: {
    backgroundColor: '#FFF8F4',
    borderRadius: 10,
    padding: 12,
    marginBottom: 16,
    flexDirection: 'row',
    alignItems: 'center',
  },
  termsText: {
    flex: 1,
    fontSize: 12,
    color: '#A0522D',
    fontFamily: 'System',
  },
  link: {
    color: '#E67E22',
    textDecorationLine: 'underline',
  },
  checkboxContainer: {
    marginLeft: 8,
  },
  checkbox: {
    width: 18,
    height: 18,
    borderWidth: 1.5,
    borderColor: '#E67E22',
    borderRadius: 4,
  },
  checked: {
    backgroundColor: '#E67E22',
  },
  signInAccount: {
    alignItems: 'center',
    marginTop: 12,
  },
  signInText: {
    color: '#A0522D',
    fontSize: 14,
    fontFamily: 'System',
  },
  signInLink: {
    color: '#E67E22',
    fontWeight: 'bold',
    textDecorationLine: 'underline',
  },
  closeButton: {
    alignSelf: 'center',
    marginTop: 12,
  },
  closeText: {
    color: '#E67E22',
    fontWeight: 'bold',
    fontSize: 16,
    fontFamily: 'System',
  },
});
